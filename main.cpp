/*
 * CEC anyway
 *
 * (C) by Magnus Kulke 2013 (mkulke at gmail dot com)
 *
 * This program is released and can be redistributed and/or modified
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "libcec/cec.h"
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <stdlib.h>
#include <unistd.h>

using namespace CEC;
using namespace std;

#define CEC_CONFIG_VERSION CEC_CLIENT_VERSION_CURRENT;

#include "libcec/cecloader.h"

ICECCallbacks        callbacks;
libcec_configuration configuration;
string               port;
bool                 aborted;
bool                 daemonize;
bool                 logEvents;                 

std::map<int, string> keyMap;

void populateKeyMap()
{
  keyMap[CEC_USER_CONTROL_CODE_LEFT]    = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"Input.Left\"}";
  keyMap[CEC_USER_CONTROL_CODE_RIGHT]   = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"Input.Right\"}";
  keyMap[CEC_USER_CONTROL_CODE_DOWN]    = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"Input.Down\"}";
  keyMap[CEC_USER_CONTROL_CODE_UP]      = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"Input.Up\"}";
  keyMap[CEC_USER_CONTROL_CODE_SELECT]  = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"Input.Select\"}";
  keyMap[CEC_USER_CONTROL_CODE_EXIT]    = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"Input.Back\"}";
  keyMap[CEC_USER_CONTROL_CODE_PLAY]    = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.PlayPause\", \"params\": { \"playerid\": 1 }, \"id\": 1}";
  keyMap[11]                            = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.Stop\", \"params\": { \"playerid\": 1 }, \"id\": 1}";
  keyMap[CEC_USER_CONTROL_CODE_PAUSE]   = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.PlayPause\", \"params\": { \"playerid\": 1 }, \"id\": 1}";
  keyMap[72]                            = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.Seek\", \"params\": { \"playerid\": 1, \"value\": \"smallbackward\" }, \"id\": 1}";
  keyMap[76]                            = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.Seek\", \"params\": { \"playerid\": 1, \"value\": \"bigbackward\" }, \"id\": 1}";
  keyMap[73]                            = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.Seek\", \"params\": { \"playerid\": 1, \"value\": \"smallforward\" }, \"id\": 1}";
  keyMap[75]                            = "{\"jsonrpc\": \"2.0\", \"method\": \"Player.Seek\", \"params\": { \"playerid\": 1, \"value\": \"bigforward\" }, \"id\": 1}";
}

int CecKeyPress(void*, const cec_keypress key)
{
  if (key.duration == 0)
  {
    string json = "unmapped";
    if (keyMap.find(key.keycode) != keyMap.end())
    {
      json = keyMap[key.keycode];

      int sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd < 0)
      {
        cout << "error opening socket" << endl;
        return 1;
      }
      
      struct sockaddr_in serv_addr; 
      memset(&serv_addr, '0', sizeof(serv_addr)); 
      serv_addr.sin_family = AF_INET;
      inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
      serv_addr.sin_port = htons(9090);
 
      if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in)) < 0)
      {
        cout << "error connecting to 127.0.0.1:9090" << endl;
        return 1;
      }

      //string json = "{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \"" + command + "\"}";

      write(sockfd, json.c_str(), json.length());   
      
      shutdown(sockfd, SHUT_WR); 
 
      close(sockfd);
    }
    
    if (logEvents)
      cout << "keycode: " << key.keycode << ", xbmc command: " << json << endl;
  }
    
  return 0;
}

void sighandler(int iSignal)
{
  cout << "signal caught: " <<  iSignal << " - exiting" << endl;
  aborted = true;
}

int main (int argc, char* argv[])
{
  daemonize = false;
  logEvents = false;
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-d") == 0) 
      daemonize = true;
    else if (strcmp(argv[i], "-l") == 0) 
      logEvents = true;
    else
    {
      cout << argv[0] << " [-d] (daemonize) [-l] (log keypresses) [-h] (help)" << endl;
      return (strcmp(argv[i], "-h") == 0) ? 0 : 1;
    }
  }
 
  if (daemonize)
  {
    pid_t pid;
    if ((pid = fork()) < 0) 
    {
      cout << "cannot fork" << endl;
      return 1;
    } 
    else if (pid != 0) 
    { 
      // parent
      exit(0);
    }
  
    setsid();
  }

  populateKeyMap();

  if (signal(SIGINT, sighandler) == SIG_ERR)
  {
    cout << "can't register sighandler" << endl;
    return -1;
  }

  configuration.Clear();
  callbacks.Clear();
  snprintf(configuration.strDeviceName, 13, "cecanyway");
  configuration.clientVersion       = CEC_CONFIG_VERSION;
  configuration.bActivateSource     = 0;
  callbacks.CBCecKeyPress           = &CecKeyPress;
  configuration.callbacks           = &callbacks;
  
  configuration.deviceTypes.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);

  ICECAdapter *parser = LibCecInitialise(&configuration);
  if (!parser)
  {
#ifdef __WINDOWS__
    cout << "Cannot load libcec.dll" << endl;
#else
    cout << "Cannot load libcec.so" << endl;
#endif
    return 1;
  }

  // init video on targets that need this
  parser->InitVideoStandalone();

  cout << "autodetect serial port: ";
  cec_adapter devices[10];
  uint8_t iDevicesFound = parser->FindAdapters(devices, 10, NULL);
  if (iDevicesFound <= 0)
  {
    cout << "FAILED" << endl;
    UnloadLibCec(parser);
    return 1;
  }
  else
  {
    port = devices[0].comm;
    cout << port << endl;
  }

  cout << "opening a connection to the CEC adapter..." << endl;

  if (!parser->Open(port.c_str()))
  {
    cout << "unable to open the device on port " << port << endl;
    UnloadLibCec(parser);
    return 1;
  }
  
  pause();

  parser->Close();

  UnloadLibCec(parser);

  return 0;
}
