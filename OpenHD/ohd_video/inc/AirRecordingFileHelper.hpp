//
// Created by consti10 on 25.10.22.
//

#ifndef OPENHD_OPENHD_OHD_VIDEO_INC_AIRRECORDINGFILEHELPER_HPP_
#define OPENHD_OPENHD_OHD_VIDEO_INC_AIRRECORDINGFILEHELPER_HPP_

#include <string>
#include <sstream>
#include "openhd-util-filesystem.hpp"
#include "openhd-util.hpp"

namespace openhd::video{

/**
 * Creates a new not yet used filename (aka the file does not yet exists) to be used for air recording.
 * @param suffix the suffix of the filename,e.g. ".avi" or ".mp4"
 */
static std::string create_unused_recording_filename(const std::string& suffix){
  if(!OHDFilesystemUtil::exists("/home/openhd/videos")){
    OHDFilesystemUtil::create_directories("/home/openhd/videos");
  }
  for(int i=0;i<10000;i++){
    std::stringstream filename;
    filename<<"/home/openhd/videos/recording";
    filename<<i<<suffix;
    if(!OHDFilesystemUtil::exists(filename.str().c_str())){
      return filename.str();
    }
  }
  std::cerr<<"Cannot create new filename, overwriting already existing\n";
  return "/home/openhd/videos/recording0"+suffix;
}

}

#endif  // OPENHD_OPENHD_OHD_VIDEO_INC_AIRRECORDINGFILEHELPER_HPP_
