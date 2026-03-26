#ifndef AUDIOHANDLERS_H
#define AUDIOHANDLERS_H

//=============================================//
//              Audio handlers                 //
//=============================================//

bool printable(const char *info) {
  if(L10N_LANGUAGE!=RU) return true;
  bool p = true;
  for (int c = 0; c < strlen(info); c++) {
    if ((uint8_t)info[c] > 0x7e || (uint8_t)info[c] < 0x20) p = false;
  }
  if (!p) p = (uint8_t)info[0] >= 0xC2 && (uint8_t)info[1] >= 0x80 && (uint8_t)info[1] <= 0xBF;
  return p;
}

void registerAudioCallbacks() {
#if I2S_DOUT!=255 || I2S_INTERNAL
  Audio::audio_info_callback = [](Audio::msg_t i) {
    switch (i.e) {

      case Audio::evt_info: {
        const char* info = i.msg;
        if (!info) break;
        if (player.lockOutput) break;
        if (config.store.audioinfo) telnet.printf("##AUDIO.INFO#: %s\n", info);
        #ifdef USE_NEXTION
          nextion.audioinfo(info);
        #endif
          if (strstr(info, "MPEG-1 Layer III") != nullptr) {
              config.setBitrateFormat(BF_MP3);
              display.putRequest(DBITRATE);
          }
          else if (strstr(info, "AAC") != nullptr) {
              config.setBitrateFormat(BF_AAC);
              display.putRequest(DBITRATE);
          }
          else if (strstr(info, "FLAC") != nullptr) {
              config.setBitrateFormat(BF_FLAC);
              display.putRequest(DBITRATE);
          }
          else if (strstr(info, "WAV") != nullptr) {
              config.setBitrateFormat(BF_WAV);
              display.putRequest(DBITRATE);
          }
          else if (strstr(info, "OGG") != nullptr || strstr(info, "VORBIS") != nullptr) {
              config.setBitrateFormat(BF_OGG);
              display.putRequest(DBITRATE);
          }
          else if (strstr(info, "OPUS") != nullptr) {
              config.setBitrateFormat(BF_OPU);
              display.putRequest(DBITRATE);
          }

          if (strstr(info, "skip metadata") != nullptr) {
              if (config.station.name[0] == '.') {
                  config.setTitle(config.station.name + 1);
              }
              else {
                  config.setTitle(config.station.name);
              }
          }
        if (strstr(info, "Account already in use") != NULL || strstr(info, "HTTP/1.0 401") != NULL) {
          player.setError(info);
        }
        break;
      }

      case Audio::evt_bitrate: {
        // i.arg1 = bitrate in bps (number extracted from msg by library)
        if (config.store.audioinfo) telnet.printf("##AUDIO.BITRATE#: %s\n", i.msg ? i.msg : "");
        if (!i.msg) {
            return;
        }
        if (config.store.audioinfo) {
            // Itt lehetne plusz log, ha szükséges
        }
        uint32_t br = static_cast<uint32_t>(atoi(i.msg));
        if (br > 3000) {
            br = br / 1000;
        }
        config.station.bitrate = br;
        display.putRequest(DBITRATE);
#ifdef USE_NEXTION
        nextion.bitrate(config.station.bitrate);
#endif
        netserver.requestOnChange(BITRATE, 0);
        break;
      }

      case Audio::evt_streamtitle: {
        const char* info = i.msg;
        if (!info) break;
        if (strstr(info, "Account already in use") != NULL ||
            strstr(info, "HTTP/1.0 401") != NULL ||
            strstr(info, "HTTP/1.1 401") != NULL) {
          player.setError(info);
          break;
        }
        bool p = printable(info) && (strlen(info) > 0);
        #ifdef DEBUG_TITLES
          config.setTitle(DEBUG_TITLES);
        #else
          config.setTitle(p ? info : config.station.name);
        #endif
        break;
      }

      case Audio::evt_name: {
        const char* info = i.msg;
        if (!info) break;
        bool p = printable(info) && (strlen(info) > 0);
        if (player.remoteStationName) {
          config.setStation(p ? info : config.station.name);
          display.putRequest(NEWSTATION);
          netserver.requestOnChange(STATION, 0);
        }
        break;
      }

      case Audio::evt_id3data: {
        if (player.lockOutput) break;
        if (i.msg) telnet.printf("##AUDIO.ID3#: %s\n", i.msg);
        if (printable(i.msg)) {
            if (strlen(config.station.title) == 0) {
                config.setTitle(i.msg);
            }
            else {
                char tmp[BUFLEN + 3];
                snprintf(tmp, BUFLEN + 3, "%s - %s", config.station.title, i.msg);
                config.setTitle(tmp);
            }
        }
        break;
      }

      case Audio::evt_eof: {
        config.sdResumePos = 0;
        player.next();
        break;
      }

      default:
        break;
    }
  };
#endif
}

// ---------- Free functions still used by the player/SD path ----------

void audio_beginSDread() {
  config.setTitle("");
}

void audio_eof_stream(const char *info) {
  player.sendCommand({PR_STOP, 0});
  if (!player.resumeAfterUrl) return;
  if (config.getMode() == PM_WEB) {
    player.sendCommand({PR_PLAY, config.lastStation()});
  } else {
    player.setResumeFilePos(config.sdResumePos == 0 ? 0 : config.sdResumePos - player.sd_min);
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
}

void audio_progress(uint32_t startpos, uint32_t endpos) {
  player.sd_min = startpos;
  player.sd_max = endpos;
  netserver.requestOnChange(SDLEN, 0);
}



#endif
