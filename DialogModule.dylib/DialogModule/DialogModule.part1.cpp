/*****************************************************************************
 
 MIT License
 
 Copyright © 2019 Samuel Venable
 
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
 
*****************************************************************************/

#include "DialogModule.h"
#include <cstdlib>
#include <string>
using std::string;

#define DIGITS_MAX 999999999999999
extern "C" const char *cocoa_dialog_caption();
extern "C" int cocoa_show_message(const char *str, bool has_cancel, const char *title);
extern "C" int cocoa_show_question(const char *str, bool has_cancel, const char *title);
extern "C" int cocoa_show_attempt(const char *str, const char *title);
extern "C" int cocoa_show_error(const char *str, bool abort, const char *title);
extern "C" const char *cocoa_input_box(const char *str, const char *def, const char *title);
extern "C" const char *cocoa_password_box(const char *str, const char *def, const char *title);
extern "C" const char *cocoa_get_open_filename(const char *filter, const char *fname, const char *dir, const char *title, const bool mselect);
extern "C" const char *cocoa_get_save_filename(const char *filter, const char *fname, const char *dir, const char *title);
extern "C" const char *cocoa_get_directory(const char *capt, const char *root);
extern "C" int cocoa_get_color(int defcol, const char *title);

static string dialog_caption;
static string error_caption;

static string remove_trailing_zeros(double numb) {
  string strnumb = std::to_string(numb);
  
  while (!strnumb.empty() && strnumb.find('.') != string::npos && (strnumb.back() == '.' || strnumb.back() == '0'))
    strnumb.pop_back();
  
  return strnumb;
}

double show_message(char *str) {
  return cocoa_show_message(str, false, dialog_caption.c_str());
}

double show_message_cancelable(char *str) {
  return cocoa_show_message(str, true, dialog_caption.c_str());
}

double show_question(char *str) {
  return cocoa_show_question(str, false, dialog_caption.c_str());
}

double show_question_cancelable(char *str) {
  return cocoa_show_question(str, true, dialog_caption.c_str());
}

double show_attempt(char *str) {
  return cocoa_show_attempt(str, error_caption.c_str());
}

double show_error(char *str, double abort) {
  double result = cocoa_show_error(str, abort, error_caption.c_str());
  if (result == 1) exit(0);
  return result;
}

char *get_string(char *str, char *def) {
  return (char *)cocoa_input_box(str, def, dialog_caption.c_str());
}

char *get_password(char *str, char *def) {
  return (char *)cocoa_password_box(str, def, dialog_caption.c_str());
}

double get_integer(char *str, double def) {
  if (def < -DIGITS_MAX) def = -DIGITS_MAX;
  if (def > DIGITS_MAX) def = DIGITS_MAX;
  string integer = remove_trailing_zeros(def);
  const char *input = cocoa_input_box(str, integer.c_str(), dialog_caption.c_str());
  
  if (input != NULL) {
    if (strtod(input, NULL) < -DIGITS_MAX) return -DIGITS_MAX;
    if (strtod(input, NULL) > DIGITS_MAX) return DIGITS_MAX;
  }
  
  return input ? strtod(input, NULL) : 0;
}

double get_passcode(char *str, double def) {
  if (def < -DIGITS_MAX) def = -DIGITS_MAX;
  if (def > DIGITS_MAX) def = DIGITS_MAX;
  string integer = remove_trailing_zeros(def);
  const char *input = cocoa_password_box(str, integer.c_str(), dialog_caption.c_str());
  
  if (input != NULL) {
    if (strtod(input, NULL) < -DIGITS_MAX) return -DIGITS_MAX;
    if (strtod(input, NULL) > DIGITS_MAX) return DIGITS_MAX;
  }
  
  return input ? strtod(input, NULL) : 0;
}

char *get_open_filename(char *filter, char *fname) {
  return (char *)cocoa_get_open_filename(filter, fname, "", "", false);
}

char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
  return (char *)cocoa_get_open_filename(filter, fname, dir, title, false);
}

char *get_open_filenames(char *filter, char *fname) {
  return (char *)cocoa_get_open_filename(filter, fname, "", "", true);
}

char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
  return (char *)cocoa_get_open_filename(filter, fname, dir, title, true);
}

char *get_save_filename(char *filter, char *fname) {
  return (char *)cocoa_get_save_filename(filter, fname, "", "");
}

char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
  return (char *)cocoa_get_save_filename(filter, fname, dir, title);
}

char *get_directory(char *dname) {
  return (char *)cocoa_get_directory("", dname);
}

char *get_directory_alt(char *capt, char *root) {
  return (char *)cocoa_get_directory(capt, root);
}

double get_color(double defcol) {
  return cocoa_get_color(defcol, "");;
}

double get_color_ext(double defcol, char *title) {
  return cocoa_get_color(defcol, title);
}

char *message_get_caption() {
  if (dialog_caption == "") dialog_caption = cocoa_dialog_caption();
  if (error_caption == "") error_caption = "Error";

  if (dialog_caption == cocoa_dialog_caption() && error_caption == "Error")
    return (char *)"";

  return (char *)dialog_caption.c_str();
}

char *message_set_caption(char *str) {
  dialog_caption = str; error_caption = str;
  if (dialog_caption == "") dialog_caption = cocoa_dialog_caption();
  if (error_caption == "") error_caption = "Error";

  if (dialog_caption == cocoa_dialog_caption() && error_caption == "Error")
    return (char *)"";

  return (char *)dialog_caption.c_str();
}

char *widget_get_system() {
    return (char *)"Cocoa";
}

char *widget_set_system(char *sys) {
    return (char *)"Cocoa";
}
