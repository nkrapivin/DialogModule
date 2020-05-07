/*

 MIT License

 Copyright © 2020 Samuel Venable
 Copyright © 2020 Robert B. Colton

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

*/

#include "DialogModule.h"
#include "lodepng.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <thread>
#include <chrono>

#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#ifdef __linux__ // Linux
#include <proc/readproc.h>
#else // BSD
#include <sys/user.h>
#include <libutil.h>
#endif

#include <sys/wait.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>
#include <signal.h>

using std::string;

namespace dialog_module {

namespace {

int const dm_x11     = -1;
int const dm_zenity  =  0;
int const dm_kdialog =  1;
int dm_dialogengine  = -1;

void *owner = NULL;
string caption;
string current_icon;

enum BUTTON_TYPES {
  BUTTON_ABORT,
  BUTTON_IGNORE,
  BUTTON_OK,
  BUTTON_CANCEL,
  BUTTON_YES,
  BUTTON_NO,
  BUTTON_RETRY
};
int const btn_array_len = 7; // number of items in BUTTON_TYPES enum.
string btn_array[btn_array_len] = { "Abort", "Ignore", "OK", "Cancel", "Yes", "No", "Retry" }; // default button names.

bool message_cancel  = false;
bool question_cancel = false;

bool dialog_position = false;
bool dialog_size     = false;

int      dialog_xpos   = 0;
int      dialog_ypos   = 0;
unsigned dialog_width  = 0;
unsigned dialog_height = 0;

void change_relative_to_kwin() {
  if (dm_dialogengine == dm_x11) {
    Display *display = XOpenDisplay(NULL);
    Atom aKWinRunning = XInternAtom(display, "KWIN_RUNNING", True);
    bool bKWinRunning = (aKWinRunning != None);
    if (bKWinRunning) dm_dialogengine = dm_kdialog;
    else dm_dialogengine = dm_zenity;
    XCloseDisplay(display);
  }
}

unsigned nlpo2dc(unsigned x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  return x | (x >> 16);
}

void XSetIcon(Display *display, Window window, const char *icon) {
  XSynchronize(display, True);
  Atom property = XInternAtom(display, "_NET_WM_ICON", True);

  unsigned char *data = nullptr;
  unsigned pngwidth, pngheight;
  unsigned error = lodepng_decode32_file(&data, &pngwidth, &pngheight, icon);
  if (error) return;

  unsigned
    widfull = nlpo2dc(pngwidth) + 1,
    hgtfull = nlpo2dc(pngheight) + 1,
    ih, iw;

  const int bitmap_size = widfull * hgtfull * 4;
  unsigned char *bitmap = new unsigned char[bitmap_size]();

  unsigned i = 0;
  unsigned elem_numb = 2 + pngwidth * pngheight;
  unsigned long *result = new unsigned long[elem_numb]();

  result[i++] = pngwidth;
  result[i++] = pngheight;
  for (ih = 0; ih < pngheight; ih++) {
    unsigned tmp = ih * widfull * 4;
    for (iw = 0; iw < pngwidth; iw++) {
      bitmap[tmp + 0] = data[4 * pngwidth * ih + iw * 4 + 2];
      bitmap[tmp + 1] = data[4 * pngwidth * ih + iw * 4 + 1];
      bitmap[tmp + 2] = data[4 * pngwidth * ih + iw * 4 + 0];
      bitmap[tmp + 3] = data[4 * pngwidth * ih + iw * 4 + 3];
      result[i++] = bitmap[tmp + 0] | (bitmap[tmp + 1] << 8) | (bitmap[tmp + 2] << 16) | (bitmap[tmp + 3] << 24);
      tmp += 4;
    }
  }

  XChangeProperty(display, window, property, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)result, elem_numb);
  XFlush(display);
  delete[] result;
  delete[] bitmap;
  delete[] data;
}

Window XGetActiveWindow(Display *display) {
  unsigned long window;
  unsigned char *prop;

  Atom actual_type, filter_atom;
  int actual_format, status;
  unsigned long nitems, bytes_after;

  int screen = XDefaultScreen(display);
  window = RootWindow(display, screen);
  if (window == 0) return 0;

  filter_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
  status = XGetWindowProperty(display, window, filter_atom, 0, 1000, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop);

  if (status == Success && prop != NULL) {
    unsigned long long_property = prop[0] + (prop[1] << 8) + (prop[2] << 16) + (prop[3] << 24);
    XFree(prop);

    return (Window)long_property;
  }
  
  return 0;
}

pid_t XGetActiveProcessId(Display *display) {
  unsigned long window = XGetActiveWindow(display);
  if (window == 0) return 0; 
  unsigned char *prop;

  Atom actual_type, filter_atom;
  int actual_format, status;
  unsigned long nitems, bytes_after;

  filter_atom = XInternAtom(display, "_NET_WM_PID", True);
  status = XGetWindowProperty(display, window, filter_atom, 0, 1000, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop);

  if (status == Success && prop != NULL) {
    unsigned long long_property = prop[0] + (prop[1] << 8) + (prop[2] << 16) + (prop[3] << 24);
    XFree(prop);

    return (pid_t)long_property;
  }
  
  return 0;
}

string string_replace_all(string str, string substr, string newstr) {
  size_t pos = 0;
  const size_t sublen = substr.length(), newlen = newstr.length();

  while ((pos = str.find(substr, pos)) != string::npos) {
    str.replace(pos, sublen, newstr);
    pos += newlen;
  }

  return str;
}

bool file_exists(string fname) {
  struct stat sb;
  return (stat(fname.c_str(), &sb) == 0 &&
    S_ISREG(sb.st_mode) != 0);
}

string filename_absolute(string fname) {
  char rpath[PATH_MAX];
  char *result = realpath(fname.c_str(), rpath);
  if (result != NULL) {
    if (file_exists(result)) return result;
  }
  return "";
}

string filename_name(string fname) {
  size_t fp = fname.find_last_of("/");
  return fname.substr(fp + 1);
}

string filename_ext(string fname) {
  fname = filename_name(fname);
  size_t fp = fname.find_last_of(".");
  if (fp == string::npos)
    return "";
  return fname.substr(fp);
}

bool WaitForChildPidOfPidToExist(pid_t pid, pid_t ppid) {
  if (pid == ppid) return false;
  while (pid != ppid) {
    if (pid <= 1) break;
    #ifdef __linux__ // Linux
    proc_t proc_info;
    memset(&proc_info, 0, sizeof(proc_info));
    PROCTAB *pt_ptr = openproc(PROC_FILLSTATUS | PROC_PID, &pid);
    if (readproc(pt_ptr, &proc_info) != 0) { 
      pid = proc_info.ppid;
    }
    closeproc(pt_ptr);
    #else // BSD
    struct kinfo_proc *proc_info = kinfo_getproc(pid);
    if (proc_info) {
      pid = proc_info->ki_ppid;
    }
    free(proc_info);
    #endif
  }
  return (pid == ppid);
}

pid_t modify_dialog(pid_t ppid) {
  pid_t pid = 0;
  if ((pid = fork()) == 0) {
    Display *display = XOpenDisplay(NULL);
    Window window, parent = owner ? (Window)owner : XGetActiveWindow(display);
    while (!WaitForChildPidOfPidToExist(XGetActiveProcessId(display), ppid));
    window = XGetActiveWindow(display);
    
    Atom window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
    Atom dialog_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", True);
    XChangeProperty(display, window, window_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&dialog_type, 1);
    XSetTransientForHint(display, window, parent);

    Atom atom_name = XInternAtom(display,"_NET_WM_NAME", True);
    Atom atom_utf_type = XInternAtom(display,"UTF8_STRING", True);
    char *cstr_caption = (char *)caption.c_str();
    XChangeProperty(display, window, atom_name, atom_utf_type, 8, PropModeReplace, (unsigned char *)cstr_caption, strlen(cstr_caption));
  
    if (file_exists(current_icon) && filename_ext(current_icon) == ".png")
      XSetIcon(display, window, current_icon.c_str());

    XCloseDisplay(display);
    exit(0);
  }
  return pid;
}

string shellscript_evaluate(string command) {
  char *buffer = NULL;
  size_t buffer_size = 0;
  string str_buffer;
  
  FILE *file = popen(command.c_str(), "r");
  pid_t ppid = getpid();
  pid_t pid = modify_dialog(ppid);
  
  while (getline(&buffer, &buffer_size, file) != -1)
    str_buffer += buffer;

  free(buffer);
  pclose(file);

  kill(pid, SIGTERM);
  bool died = false;

  for (unsigned i = 0; !died && i < 4; i++) {
    int status; 
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (waitpid(pid, &status, WNOHANG) == pid) died = true;
  }

  if (!died) kill(pid, SIGKILL);
  if (str_buffer[str_buffer.length() - 1] == '\n')
    str_buffer.pop_back();

  return str_buffer;
}

string add_escaping(string str, bool is_caption, string new_caption) {
  string result = str; if (is_caption && str == "") result = new_caption;
  result = string_replace_all(result, "\"", "\\\"");

  if (dm_dialogengine == dm_zenity)
    result = string_replace_all(result, "_", "__");

  return result;
}

string remove_trailing_zeros(double numb) {
  string strnumb = std::to_string(numb);

  while (!strnumb.empty() && strnumb.find('.') != string::npos && (strnumb.back() == '.' || strnumb.back() == '0'))
    strnumb.pop_back();

  return strnumb;
}

std::vector<string> string_split(const string &str, char delimiter) {
  std::vector<string> vec;
  std::stringstream sstr(str);
  string tmp;

  while (std::getline(sstr, tmp, delimiter))
    vec.push_back(tmp);

  return vec;
}

string zenity_filter(string input) {
  std::vector<string> stringVec = string_split(input, '|');
  string string_output = "";

  unsigned index = 0;
  for (string str : stringVec) {
    if (index % 2 == 0)
      string_output += " --file-filter='" + string_replace_all(str, "*.*", "*") + "|";
    else {
      std::replace(str.begin(), str.end(), ';', ' ');
      string_output += string_replace_all(str, "*.*", "*") + "'";
    }

    index += 1;
  }

  return string_output;
}

string kdialog_filter(string input) {
  std::vector<string> stringVec = string_split(input, '|');
  string string_output = " '";

  unsigned index = 0;
  for (string str : stringVec) {
    if (index % 2 == 0) {
      if (index != 0)
        string_output += "\n";
      size_t first = str.find('(');
      if (first != string::npos) {
        size_t last = str.find(')', first);
        if (last != string::npos)
          str.erase(first, last - first + 1);
      }
      string_output += str + " (";
    } else {
      std::replace(str.begin(), str.end(), ';', ' ');
      string_output += string_replace_all(str, "*.*", "*") + ")";
    }

    index += 1;
  }

  string_output += "'";
  return string_output;
}

int color_get_red(int col) { return ((col & 0x000000FF)); }
int color_get_green(int col) { return ((col & 0x0000FF00) >> 8); }
int color_get_blue(int col) { return ((col & 0x00FF0000) >> 16); }

int make_color_rgb(unsigned char r, unsigned char g, unsigned char b) {
  return r | (g << 8) | (b << 16);
}

int show_message_helperfunc(char *str) {  
  change_relative_to_kwin();
  string str_command;
  string str_title = message_cancel ? add_escaping(caption, true, "Question") : add_escaping(caption, true, "Information");
  string caption_previous = caption;
  caption = (str_title == "Information") ? "Information" : caption;
  caption = (str_title == "Question") ? "Question" : caption;

  string str_cancel;
  string str_echo = "echo 1";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") +
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (message_cancel)
    str_echo = "if [ $? = 0 ] ;then echo 1;else echo -1;fi";

  if (dm_dialogengine == dm_zenity) {
    string str_icon = "\" --icon-name=dialog-information);";
    str_cancel = "--info --ok-label=" + btn_array[BUTTON_OK] + " ";

    if (message_cancel) {
      str_icon = "\" --icon-name=dialog-question);";
      str_cancel = "--question --ok-label=" + btn_array[BUTTON_OK] + " --cancel-label=" + btn_array[BUTTON_CANCEL] + " ";
    }

    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    str_cancel + string("--title=\"") + str_title + string("\" --no-wrap --text=\"") +
    add_escaping(str, false, "") + str_icon + str_echo;
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_cancel = string("--msgbox \"") + add_escaping(str, false, "") + string("\" --icon dialog-information ");

    if (message_cancel)
      str_cancel = string("--yesno \"") + add_escaping(str, false, "") + string("\" --yes-label " + btn_array[BUTTON_OK] + " --no-label " + btn_array[BUTTON_CANCEL] + " --icon dialog-question ");

    str_command = string("kdialog ") +
    string("--attach=") + window + string(" ") +
    str_cancel + string("--title \"") + str_title + string("\";") + str_echo;
  }

  string str_result = shellscript_evaluate(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), NULL);
  return (int)result;
}

int show_question_helperfunc(char *str) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Question");
  string caption_previous = caption;
  caption = (str_title == "Question") ? "Question" : caption;
  string str_cancel = "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    if (question_cancel)
      str_cancel = "--extra-button=" + btn_array[BUTTON_CANCEL] + " ";

    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--question --ok-label=" + btn_array[BUTTON_YES] + " --cancel-label=" + btn_array[BUTTON_NO] + " ") + str_cancel +  string("--title=\"") +
    str_title + string("\" --no-wrap --text=\"") + add_escaping(str, false, "") +
    string("\" --icon-name=dialog-question);if [ $? = 0 ] ;then echo 1;elif [ $ans = \"" + btn_array[BUTTON_CANCEL] + "\" ] ;then echo -1;else echo 0;fi");
  }
  else if (dm_dialogengine == dm_kdialog) {
    if (question_cancel)
      str_cancel = "cancel";

    str_command = string("kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--yesno") + str_cancel + string(" \"") + add_escaping(str, false, "") + string("\" ") +
    string("--yes-label " + btn_array[BUTTON_YES] + " --no-label " + btn_array[BUTTON_NO] + " ") + string("--title \"") + str_title + string("\" --icon dialog-question;") +
    string("x=$? ;if [ $x = 0 ] ;then echo 1;elif [ $x = 1 ] ;then echo 0;elif [ $x = 2 ] ;then echo -1;fi");
  }

  string str_result = shellscript_evaluate(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), NULL);
  return (int)result;
}

} // anonymous namespace

int show_message(char *str) {
  message_cancel = false;
  return show_message_helperfunc(str);
}

int show_message_cancelable(char *str) {
  message_cancel = true;
  return show_message_helperfunc(str);
}

int show_question(char *str) {
  question_cancel = false;
  return show_question_helperfunc(str);
}

int show_question_cancelable(char *str) {
  question_cancel = true;
  return show_question_helperfunc(str);
}

int show_attempt(char *str) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Error");
  string caption_previous = caption;
  caption = (str_title == "Error") ? "Error" : caption;
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--question --ok-label=" + btn_array[BUTTON_RETRY] + " --cancel-label=" + btn_array[BUTTON_CANCEL] + " ") +  string("--title=\"") +
    str_title + string("\" --no-wrap --text=\"") + add_escaping(str, false, "") +
    string("\" --icon-name=dialog-error --window-icon=dialog-error);if [ $? = 0 ] ;then echo 0;else echo -1;fi");
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_command = string("kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--warningyesno") + string(" \"") + add_escaping(str, false, "") + string("\" ") +
    string("--yes-label " + btn_array[BUTTON_RETRY] + " --no-label " + btn_array[BUTTON_CANCEL] + " ") + string("--title \"") +
    str_title + string("\" --icon dialog-warning;") + string("x=$? ;if [ $x = 0 ] ;then echo 0;else echo -1;fi");
  }

  string str_result = shellscript_evaluate(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), NULL);
  return (int)result;
}

int show_error(char *str, bool abort) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Error");
  string caption_previous = caption;
  caption = (str_title == "Error") ? "Error" : caption;
  string str_echo;
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_echo = abort ? "echo 1" : "if [ $? = 0 ] ;then echo 1;else echo -1;fi";

    if (abort) {
      str_command = string("ans=$(zenity ") +
      string("--attach=$(sleep .01;") + window + string(") ") +
      string("--info --ok-label=" + btn_array[BUTTON_ABORT] + " ") +
      string("--title=\"") + str_title + string("\" --no-wrap --text=\"") +
      add_escaping(str, false, "") + string("\" --icon-name=dialog-error --window-icon=dialog-error);") + str_echo;
    } else {
      str_command = string("ans=$(zenity ") +
      string("--attach=$(sleep .01;") + window + string(") ") +
      string("--question --ok-label=" + btn_array[BUTTON_ABORT] + " --cancel-label=" + btn_array[BUTTON_IGNORE] + " ") +
      string("--title=\"") + str_title + string("\" --no-wrap --text=\"") +
      add_escaping(str, false, "") + string("\" --icon-name=dialog-error --window-icon=dialog-error);") + str_echo;
    }
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_echo = abort ? "echo 1" : "x=$? ;if [ $x = 0 ] ;then echo 1;elif [ $x = 1 ] ;then echo -1;fi";

    if (abort) {
      str_command = string("kdialog ") +
      string("--attach=") + window + string(" ") +
      string("--sorry \"") + add_escaping(str, false, "") + string("\" ") +
      string("--ok-label " + btn_array[BUTTON_ABORT] + " ") +
      string("--title \"") + str_title + string("\" --icon dialog-warning;") + str_echo;
    } else {
      str_command = string("kdialog ") +
      string("--attach=") + window + string(" ") +
      string("--warningyesno \"") + add_escaping(str, false, "") + string("\" ") +
      string("--yes-label " + btn_array[BUTTON_ABORT] + " --no-label " + btn_array[BUTTON_IGNORE] + " ") +
      string("--title \"") + str_title + string("\" --icon dialog-warning;") + str_echo;
    }
  }

  string str_result = shellscript_evaluate(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), NULL);
  if (result == 1) exit(0);
  return (int)result;
}

char *get_string(char *str, char *def) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Input Query");
  string caption_previous = caption;
  caption = (str_title == "Input Query") ? "Input Query" : caption;
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? string(" --icon \"") + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--entry --title=\"") + str_title + string("\" --text=\"") +
    add_escaping(str, false, "") + string("\" --entry-text=\"") +
    add_escaping(def, false, "") + string("\");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--inputbox \"") + add_escaping(str, false, "") + string("\" \"") +
    add_escaping(def, false, "") + string("\" --title \"") +
    str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

char *get_password(char *str, char *def) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Input Query");
  string caption_previous = caption;
  caption = (str_title == "Input Query") ? "Input Query" : caption;
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--entry --title=\"") + str_title + string("\" --text=\"") +
    add_escaping(str, false, "") + string("\" --hide-text --entry-text=\"") +
    add_escaping(def, false, "") + string("\");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--password \"") + add_escaping(str, false, "") + string("\" \"") +
    add_escaping(def, false, "") + string("\" --title \"") +
    str_title + string("\");echo $ans");
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

double get_integer(char *str, double def) {
  double DIGITS_MIN = -999999999999999;
  double DIGITS_MAX = 999999999999999;

  if (def < DIGITS_MIN) def = DIGITS_MIN;
  if (def > DIGITS_MAX) def = DIGITS_MAX;

  string str_def = remove_trailing_zeros(def);
  string str_result = get_string(str, (char *)str_def.c_str());
  double result = strtod(str_result.c_str(), NULL);

  if (result < DIGITS_MIN) result = DIGITS_MIN;
  if (result > DIGITS_MAX) result = DIGITS_MAX;
  return result;
}

double get_passcode(char *str, double def) {
  double DIGITS_MIN = -999999999999999;
  double DIGITS_MAX = 999999999999999;

  if (def < DIGITS_MIN) def = DIGITS_MIN;
  if (def > DIGITS_MAX) def = DIGITS_MAX;

  string str_def = remove_trailing_zeros(def);
  string str_result = get_password(str, (char *)str_def.c_str());
  double result = strtod(str_result.c_str(), NULL);

  if (result < DIGITS_MIN) result = DIGITS_MIN;
  if (result > DIGITS_MAX) result = DIGITS_MAX;
  return result;
}

char *get_open_filename(char *filter, char *fname) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = "Open";
  string caption_previous = caption;
  caption = str_title;
  string str_fname = basename(fname);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + add_escaping(zenity_filter(filter), false, "") + str_icon + string(");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getopenfilename ") + pwd + add_escaping(kdialog_filter(filter), false, "") +
    string(" --title \"") + str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;

  if (file_exists(result))
    return (char *)result.c_str();

  return (char *)"";
}

char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(title, true, "Open");
  string caption_previous = caption;
  caption = (str_title == "Open") ? "Open" : title;
  string str_fname = basename(fname);
  string str_dir = dirname(dir);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  string str_path = fname;
  if (str_dir[0] != '\0') str_path = str_dir + string("/") + str_fname;
  str_fname = (char *)str_path.c_str();

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + add_escaping(zenity_filter(filter), false, "") + str_icon + string(");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getopenfilename ") + pwd + add_escaping(kdialog_filter(filter), false, "") +
    string(" --title \"") + str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;

  if (file_exists(result))
    return (char *)result.c_str();

  return (char *)"";
}

char *get_open_filenames(char *filter, char *fname) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = "Open";
  string caption_previous = caption;
  caption = str_title;
  string str_fname = basename(fname);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection --multiple --separator='\n' --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + add_escaping(zenity_filter(filter), false, "") + str_icon;
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getopenfilename ") + pwd + add_escaping(kdialog_filter(filter), false, "") +
    string(" --multiple --separate-output --title \"") + str_title + string("\"") + str_icon;
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  std::vector<string> stringVec = string_split(result, '\n');

  bool success = true;
  for (const string &str : stringVec) {
    if (!file_exists(str))
      success = false;
  }

  if (success)
    return (char *)result.c_str();

  return (char *)"";
}

char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(title, true, "Open");
  string caption_previous = caption;
  caption = (str_title == "Open") ? "Open" : title;
  string str_fname = basename(fname);
  string str_dir = dirname(dir);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  string str_path = fname;
  if (str_dir[0] != '\0') str_path = str_dir + string("/") + str_fname;
  str_fname = (char *)str_path.c_str();

  if (dm_dialogengine == dm_zenity) {
    str_command = string("zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection --multiple --separator='\n' --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + add_escaping(zenity_filter(filter), false, "") + str_icon;
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getopenfilename ") + pwd + add_escaping(kdialog_filter(filter), false, "") +
    string(" --multiple --separate-output --title \"") + str_title + string("\"") + str_icon;
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  std::vector<string> stringVec = string_split(result, '\n');

  bool success = true;
  for (const string &str : stringVec) {
    if (!file_exists(str))
      success = false;
  }

  if (success)
    return (char *)result.c_str();

  return (char *)"";
}

char *get_save_filename(char *filter, char *fname) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = "Save As";
  string caption_previous = caption;
  caption = str_title;
  string str_fname = basename(fname);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection  --save --confirm-overwrite --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + add_escaping(zenity_filter(filter), false, "") + str_icon + string(");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getsavefilename ") + pwd + add_escaping(kdialog_filter(filter), false, "") +
    string(" --title \"") + str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(title, true, "Save As");
  string caption_previous = caption;
  caption = (str_title == "Save As") ? "Save As" : title;
  string str_fname = basename(fname);
  string str_dir = dirname(dir);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  string str_path = fname;
  if (str_dir[0] != '\0') str_path = str_dir + string("/") + str_fname;
  str_fname = (char *)str_path.c_str();

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection  --save --confirm-overwrite --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + add_escaping(zenity_filter(filter), false, "") + str_icon + string(");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getsavefilename ") + pwd + add_escaping(kdialog_filter(filter), false, "") +
    string(" --title \"") + str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

char *get_directory(char *dname) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = "Select Directory";
  string caption_previous = caption;
  caption = str_title;
  string str_dname = dname;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  string str_end = ");if [ $ans = / ] ;then echo $ans;elif [ $? = 1 ] ;then echo $ans/;else echo $ans;fi";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection --directory --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_dname, false, "") + string("\"") + str_icon + str_end;
  }
  else if (dm_dialogengine == dm_kdialog) {
    if (str_dname.c_str() && str_dname[0] != '/' && str_dname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_dname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getexistingdirectory ") + pwd + string(" --title \"") + str_title + string("\"") + str_icon + str_end;
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

char *get_directory_alt(char *capt, char *root) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(capt, true, "Select Directory");
  string caption_previous = caption;
  caption = (str_title == "Select Directory") ? "Select Directory" : capt;
  string str_dname = root;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  string str_end = ");if [ $ans = / ] ;then echo $ans;elif [ $? = 1 ] ;then echo $ans/;else echo $ans;fi";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--file-selection --directory --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_dname, false, "") + string("\"") + str_icon + str_end;
  }
  else if (dm_dialogengine == dm_kdialog) {
    if (str_dname.c_str() && str_dname[0] != '/' && str_dname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_dname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getexistingdirectory ") + pwd + string(" --title \"") + str_title + string("\"") + str_icon + str_end;
  }

  static string result;
  result = shellscript_evaluate(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

int get_color(int defcol) {
  change_relative_to_kwin();
  string str_command;
  string str_title = "Color";
  string caption_previous = caption;
  caption = str_title;
  string str_defcol;
  string str_result;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  int red; int green; int blue;
  red = color_get_red(defcol);
  green = color_get_green(defcol);
  blue = color_get_blue(defcol);

  if (dm_dialogengine == dm_zenity) {
    str_defcol = string("rgb(") + std::to_string(red) + string(",") +
    std::to_string(green) + string(",") + std::to_string(blue) + string(")");
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--color-selection --show-palette --title=\"") + str_title + string("\"  --color='") +
    str_defcol + string("'") + str_icon + string(");if [ $? = 0 ] ;then echo $ans;else echo -1;fi");

    str_result = shellscript_evaluate(str_command);
    caption = caption_previous;
    if (str_result == "-1") return strtod(str_result.c_str(), NULL);
    str_result = string_replace_all(str_result, "rgba(", "");
    str_result = string_replace_all(str_result, "rgb(", "");
    str_result = string_replace_all(str_result, ")", "");
    std::vector<string> stringVec = string_split(str_result, ',');

    unsigned int index = 0;
    for (const string &str : stringVec) {
      if (index == 0) red = strtod(str.c_str(), NULL);
      if (index == 1) green = strtod(str.c_str(), NULL);
      if (index == 2) blue = strtod(str.c_str(), NULL);
      index += 1;
    }

  } else if (dm_dialogengine == dm_kdialog) {
    char hexcol[16];
    snprintf(hexcol, sizeof(hexcol), "%02x%02x%02x", red, green, blue);

    str_defcol = string("#") + string(hexcol);
    std::transform(str_defcol.begin(), str_defcol.end(), str_defcol.begin(), ::toupper);

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getcolor --default '") + str_defcol + string("' --title \"") + str_title +
    string("\"") + str_icon + string(");if [ $? = 0 ] ;then echo $ans;else echo -1;fi");

    str_result = shellscript_evaluate(str_command);
    caption = caption_previous;
    if (str_result == "-1") return strtod(str_result.c_str(), NULL);
    str_result = str_result.substr(1, str_result.length() - 1);

    unsigned int color;
    std::stringstream ss2;
    ss2 << std::hex << str_result;
    ss2 >> color;

    blue = color_get_red(color);
    green = color_get_green(color);
    red = color_get_blue(color);
  }

  return (int)make_color_rgb(red, green, blue);
}

int get_color_ext(int defcol, char *title) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(title, true, "Color");
  string caption_previous = caption;
  caption = (str_title == "Color") ? "Color" : title;
  string str_defcol;
  string str_result;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + current_icon + string("\"") : "";
  
  Display *display = XOpenDisplay(NULL);
  string window = owner ? ((dm_dialogengine == dm_zenity) ? "echo " : "") + 
    std::to_string((unsigned long)owner) : 
    std::to_string((unsigned long)XGetActiveWindow(display));
  XCloseDisplay(display);

  int red; int green; int blue;
  red = color_get_red(defcol);
  green = color_get_green(defcol);
  blue = color_get_blue(defcol);

  if (dm_dialogengine == dm_zenity) {
    str_defcol = string("rgb(") + std::to_string(red) + string(",") +
    std::to_string(green) + string(",") + std::to_string(blue) + string(")");
    str_command = string("ans=$(zenity ") +
    string("--attach=$(sleep .01;") + window + string(") ") +
    string("--color-selection --show-palette --title=\"") + str_title + string("\" --color='") +
    str_defcol + string("'") + str_icon + string(");if [ $? = 0 ] ;then echo $ans;else echo -1;fi");

    str_result = shellscript_evaluate(str_command);
    caption = caption_previous;
    if (str_result == "-1") return strtod(str_result.c_str(), NULL);
    str_result = string_replace_all(str_result, "rgba(", "");
    str_result = string_replace_all(str_result, "rgb(", "");
    str_result = string_replace_all(str_result, ")", "");
    std::vector<string> stringVec = string_split(str_result, ',');

    unsigned int index = 0;
    for (const string &str : stringVec) {
      if (index == 0) red = strtod(str.c_str(), NULL);
      if (index == 1) green = strtod(str.c_str(), NULL);
      if (index == 2) blue = strtod(str.c_str(), NULL);
      index += 1;
    }

  } else if (dm_dialogengine == dm_kdialog) {
    char hexcol[16];
    snprintf(hexcol, sizeof(hexcol), "%02x%02x%02x", red, green, blue);

    str_defcol = string("#") + string(hexcol);
    std::transform(str_defcol.begin(), str_defcol.end(), str_defcol.begin(), ::toupper);

    str_command = string("ans=$(kdialog ") +
    string("--attach=") + window + string(" ") +
    string("--getcolor --default '") + str_defcol + string("' --title \"") + str_title +
    string("\"") + str_icon + string(");if [ $? = 0 ] ;then echo $ans;else echo -1;fi");

    str_result = shellscript_evaluate(str_command);
    caption = caption_previous;
    if (str_result == "-1") return strtod(str_result.c_str(), NULL);
    str_result = str_result.substr(1, str_result.length() - 1);

    unsigned int color;
    std::stringstream ss2;
    ss2 << std::hex << str_result;
    ss2 >> color;

    blue = color_get_red(color);
    green = color_get_green(color);
    red = color_get_blue(color);
  }

  return (int)make_color_rgb(red, green, blue);
}

char *widget_get_caption() {
  return (char *)caption.c_str();
}

void widget_set_caption(char *title) {
  caption = title ? title : "";
}

void *widget_get_owner() {
  return owner;
}

void widget_set_owner(void *hwnd) {
  owner = hwnd;
}

char *widget_get_icon() {
  if (current_icon == "") 
    current_icon = filename_absolute("assets/icon.png");
  return (char *)current_icon.c_str();
}

void widget_set_icon(char *icon) {
  current_icon = filename_absolute(icon);
}

char *widget_get_system() {
  if (dm_dialogengine == dm_zenity)
    return (char *)"Zenity";

  if (dm_dialogengine == dm_kdialog)
    return (char *)"KDialog";

  return (char *)"X11";
}

void widget_set_system(char *sys) {
  string str_sys = sys;
  
  if (str_sys == "X11")
    dm_dialogengine = dm_x11;

  if (str_sys == "Zenity")
    dm_dialogengine = dm_zenity;

  if (str_sys == "KDialog")
    dm_dialogengine = dm_kdialog;
}

void widget_set_button_name(double type, char *name) {
  string str_name = name;
  
  btn_array[(int)type] = str_name;
}

char *widget_get_button_name(double type) {
  return strdup(btn_array[(int)type].c_str());
}

} // namepace dialog_module
