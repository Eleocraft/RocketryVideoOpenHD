//
// Created by consti10 on 02.05.22.
//

#include <getopt.h>
#include <ohd_interface.h>
#ifdef ENABLE_AIR
#include <ohd_video_air.h>
#endif  // ENABLE_AIR
#include <ohd_video_ground.h>

#include <csignal>
#include <iostream>
#include <memory>

#include "openhd_buttons.h"
#include "openhd_global_constants.hpp"
#include "openhd_platform.h"
#include "openhd_profile.h"
#include "openhd_spdlog.h"
#include "openhd_temporary_air_or_ground.h"
// For logging the commit hash and more
// #include "git.h"
#include "openhd_config.h"

// |-------------------------------------------------------------------------------|
// |                         OpenHD core executable | | Weather you run as air
// (creates openhd air unit) or run as ground             | | (creates openhd
// ground unit) needs to be specified by either using the command| | line param
// (development) or using a text file (openhd images)                 | | Read
// the code documentation in this project for more info.                    |
// |-------------------------------------------------------------------------------|

// A few run time options, only for development. Way more configuration (during
// development) can be done by using the hardware.config file
static const char optstr[] = "?:agcr:h:";
static const struct option long_options[] = {
    {"air", no_argument, nullptr, 'a'},
    {"ground", no_argument, nullptr, 'g'},
    {"clean-start", no_argument, nullptr, 'c'},
    {"run-time-seconds", required_argument, nullptr, 'r'},
    {"hardware-config-file", required_argument, nullptr, 'h'},
    {nullptr, 0, nullptr, 0},
};

struct OHDRunOptions {
  bool run_as_air = false;
  bool reset_all_settings = false;
  int run_time_seconds = -1;  //-1= infinite, only usefully for debugging
  // Specify the hardware.config file, otherwise,
  // the default location (and default values if no file exists at the default
  // location) is used
  std::optional<std::string> hardware_config_file;
};

static OHDRunOptions parse_run_parameters(int argc, char *argv[]) {
  OHDRunOptions ret{};
  int c;
  // If this value gets set, we assume a developer is working on OpenHD and skip
  // the discovery via file(s).
  std::optional<bool> commandline_air = std::nullopt;
  while ((c = getopt_long(argc, argv, optstr, long_options, nullptr)) != -1) {
    const char *tmp_optarg = optarg;
    switch (c) {
      case 'a':
        if (commandline_air != std::nullopt) {
          // Already set, e.g. --ground is already used
          std::cerr << "Please use either air or ground as param\n";
          exit(1);
        }
        commandline_air = true;
        break;
      case 'g':
        // Already set, e.g. --air is already used
        if (commandline_air != std::nullopt) {
          std::cerr << "Please use either air or ground as param\n";
          exit(1);
        }
        commandline_air = false;
        break;
      case 'c':
        ret.reset_all_settings = true;
        break;
      case 'r':
        ret.run_time_seconds = atoi(tmp_optarg);
        break;
      case 'h':
        ret.hardware_config_file = tmp_optarg;
        break;
      case '?':
      default: {
        std::stringstream ss;
        ss << "Usage: \n";
        ss << "--air -a          [Run as air, creates dummy camera if no "
              "camera is found] \n";
        ss << "--ground -g       [Run as ground, no camera detection] \n";
        ss << "--clean-start -c  [Wipe all persistent settings OpenHD has "
              "written, can fix any boot issues when switching hw around] \n";
        ss << "--run-time-seconds -r [Manually specify run time (default "
              "infinite),for debugging] \n";
        ss << "--hardware-config-file -h [specify path to hardware.config "
              "file]\n";
        ss << "Use /boot/openhd/hardware.config for more configuration\n";
        std::cout << ss.str() << std::flush;
      }
        exit(1);
    }
  }
  if (commandline_air == std::nullopt) {
    // command line parameters not used, use the file(s) for detection (default
    // for normal OpenHD images) The logs/checks here are just to help
    // developer(s) avoid common misconfigurations
    std::cout << "Using files to detect air or ground\n";
    const bool file_run_as_ground_exists = openhd::tmp::file_ground_exists();
    const bool file_run_as_air_exists = openhd::tmp::file_air_exists();
    bool error = false;
    if (file_run_as_air_exists &&
        file_run_as_ground_exists) {  // both files exist
      std::cerr << "Both air and ground files exist,unknown what you want - "
                   "either use the command line param or delete one of them\n";
      std::cerr << "Assuming ground\n";
      // Just run as ground
      ret.run_as_air = false;
      error = true;
    }
    if (!file_run_as_air_exists &&
        !file_run_as_ground_exists) {  // no file exists
      std::cerr << "No file air or ground exists,unknown what you want - "
                   "either use the command line param or create a file\n";
      std::cerr << "Assuming ground\n";
      // Just run as ground
      ret.run_as_air = false;
      error = true;
    }
    if (!error) {
      if (!file_run_as_air_exists) {
        ret.run_as_air = false;
      } else {
        ret.run_as_air = true;
      }
    }
  } else {
    // command line parameters used, just validate they are not mis-configured
    assert(commandline_air.has_value());
    ret.run_as_air = commandline_air.value();
  }
  // If this file exists, delete all openhd settings resulting in default
  // value(s)
  static constexpr auto FILE_PATH_RESET = "/boot/openhd/reset.txt";
  if (OHDUtil::file_exists_and_delete(FILE_PATH_RESET)) {
    ret.reset_all_settings = true;
  }
#ifndef ENABLE_AIR
  if (ret.run_as_air) {
    std::cerr << "NOTE: COMPILED WITH GROUND ONLY SUPPORT,RUNNING AS GND"
              << std::endl;
    ret.run_as_air = false;
  }
#endif
  return ret;
}

int main(int argc, char *argv[]) {
  // OpenHD needs to be run as root, otherwise we cannot access/ modify the
  // Wi-Fi cards for example (And there are also many other places where we just
  // need to be root).
  OHDUtil::terminate_if_not_root();

  // Create the folder structure for the (per-module-specific) settings if
  // needed
  openhd::generateSettingsDirectoryIfNonExists();

  // Generate the keys and delete pw if needed
  OHDInterface::generate_keys_from_pw_if_exists_and_delete();

  // Parse the program arguments, also uses the "yes if file exists" pattern for
  // some params
  const OHDRunOptions options = parse_run_parameters(argc, argv);
  if (options.hardware_config_file.has_value()) {
    openhd::set_config_file(options.hardware_config_file.value());
  }
  {  // Print all the arguments the OHD main executable is started with
    std::stringstream ss;
    ss << "OpenHD START with \n";
    ss << "air:" << OHDUtil::yes_or_no(options.run_as_air) << "\n";
    ss << "reset_all_settings:"
       << OHDUtil::yes_or_no(options.reset_all_settings) << "\n";
    ss << "run_time_seconds:" << options.run_time_seconds << "\n";
    ss << "hardware-config-file:["
       << options.hardware_config_file.value_or("DEFAULT") << "]\n";
    ss << "Version number:" << openhd::get_ohd_version_as_string() << "\n";
    ss << "This version of OpenHD is operated by WueSpace e. v. " << "\n";
    std::cout << ss.str() << std::flush;
    openhd::debug_config();
    OHDInterface::print_internal_fec_optimization_method();
  }
  // This is the console we use inside main, in general different openhd
  // modules/classes have their own loggers with different tags
  std::shared_ptr<spdlog::logger> m_console =
      openhd::log::create_or_get("main");
  assert(m_console);

  // not guaranteed, but better than nothing, check if openhd is already running
  // (kinda) and print warning if yes.
  openhd::check_currently_running_file_and_write();

  // First discover the platform -
  const auto platform = OHDPlatform::instance();
  openhd::LEDManager::instance().set_status_loading();

  // Create and link all the OpenHD modules.
  try {
    // This results in fresh default values for all modules (e.g. interface,
    // video)
    if (options.reset_all_settings) {
      openhd::clean_all_settings();
    }
    if (openhd::ButtonManager::instance().user_wants_reset_openhd_core()) {
      openhd::clean_all_settings();
    }
    // Profile no longer depends on n discovered cameras,
    // But if we are air, we have at least one camera, sw if no camera was found
    const auto profile = DProfile::discover(options.run_as_air);
    write_profile_manifest(profile);

    // create the global action handler that allows openhd modules to
    // communicate with each other e.g. when the rf link in ohd_interface needs
    // to talk to the camera streams to reduce the bitrate
    openhd::LinkActionHandler::instance();

    // Then start ohdInterface, which discovers detected wifi cards and more.
    auto ohdInterface = std::make_shared<OHDInterface>(profile);

    // either one is active, depending on air or ground
    std::unique_ptr<OHDVideoGround> ohd_video_ground = nullptr;
    if (profile.is_ground()) {
      ohd_video_ground =
          std::make_unique<OHDVideoGround>(ohdInterface->get_link_handle());
    }
#ifdef ENABLE_AIR
    std::unique_ptr<OHDVideoAir> ohd_video_air = nullptr;
    if (profile.is_air) {
      auto cameras = OHDVideoAir::discover_cameras();
      ohd_video_air = std::make_unique<OHDVideoAir>(
          cameras, ohdInterface->get_link_handle());
    }
#endif  // ENABLE_AIR
    m_console->info("All OpenHD modules running");
    openhd::LEDManager::instance().set_status_okay();
    openhd::log::log_to_kernel("All OpenHD modules running");

    // run forever, everything has its own threads. Note that the only way to
    // break out basically is when one of the modules encounters an exception.
    static bool quit = false;
    // https://unix.stackexchange.com/questions/362559/list-of-terminal-generated-signals-eg-ctrl-c-sigint
    signal(SIGTERM, [](int sig) {
      std::cerr << "Got SIGTERM, exiting\n";
      quit = true;
    });
    signal(SIGQUIT, [](int sig) {
      std::cerr << "Got SIGQUIT, exiting\n";
      quit = true;
    });
    const auto run_time_begin = std::chrono::steady_clock::now();
    while (!quit) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      if (options.run_time_seconds >= 1) {
        if (std::chrono::steady_clock::now() - run_time_begin >=
            std::chrono::seconds(options.run_time_seconds)) {
          m_console->warn("Terminating, exceeded run time {}",
                          options.run_time_seconds);
          // we can just break out any time, usefully for checking memory leaks
          // and more.
          break;
        }
      }
      if (openhd::TerminateHelper::instance().should_terminate()) {
        m_console->debug("Terminating,reason:{}",
                         openhd::TerminateHelper::instance().terminate_reason());
        break;
      }
    }
    // --- terminate openhd, most likely requested by a developer with sigterm
    m_console->debug("Terminating openhd");
    // Stop any communication between modules, to eliminate any issues created
    // by threads during cleanup
    openhd::LinkActionHandler::instance().disable_all_callables();
    openhd::ExternalDeviceManager::instance().remove_all();
    // dirty, wait a bit to make sure none of those action(s) are called anymore
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // unique ptr would clean up for us, but this way we are a bit more verbose
    // since some of those modules talk to each other, this is a bit prone to
    // failures.
#ifdef ENABLE_AIR
    if (ohd_video_air) {
      m_console->debug("Terminating ohd_video_air - begin");
      ohd_video_air.reset();
      m_console->debug("Terminating ohd_video_air - end");
    }
#endif
    if (ohd_video_ground) {
      m_console->debug("Terminating ohd_video_ground- begin");
      ohd_video_ground.reset();
      m_console->debug("Terminating ohd_video_ground - end");
    }
    if (ohdInterface) {
      m_console->debug("Terminating ohd_interface - begin");
      ohdInterface.reset();
      m_console->debug("Terminating ohd_interface - end");
    }
  } catch (std::exception &ex) {
    std::cerr << "Error: " << ex.what() << std::endl;
    exit(1);
  } catch (...) {
    std::cerr << "Unknown exception occurred" << std::endl;
    exit(1);
  }
  openhd::remove_currently_running_file();
  return 0;
}
