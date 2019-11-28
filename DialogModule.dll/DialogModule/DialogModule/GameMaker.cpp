/*
 
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
 
 */

#include "DialogModule.h"

#ifdef _WIN32
#define EXPORTED_FUNCTION extern "C" __declspec(dllexport)
#else /* macOS, Linux, and BSD */
#define EXPORTED_FUNCTION extern "C" __attribute__((visibility("default")))
#endif

EXPORTED_FUNCTION double show_message(char *str);
EXPORTED_FUNCTION double show_message_cancelable(char *str);
EXPORTED_FUNCTION double show_question(char *str);
EXPORTED_FUNCTION double show_question_cancelable(char *str);
EXPORTED_FUNCTION double show_attempt(char *str);
EXPORTED_FUNCTION double show_error(char *str, double abort);
EXPORTED_FUNCTION char *get_string(char *str, char *def);
EXPORTED_FUNCTION char *get_password(char *str, char *def);
EXPORTED_FUNCTION double get_integer(char *str, double def);
EXPORTED_FUNCTION double get_passcode(char *str, double def);
EXPORTED_FUNCTION char *get_open_filename(char *filter, char *fname);
EXPORTED_FUNCTION char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION char *get_open_filenames(char *filter, char *fname);
EXPORTED_FUNCTION char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION char *get_save_filename(char *filter, char *fname);
EXPORTED_FUNCTION char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION char *get_directory(char *dname);
EXPORTED_FUNCTION char *get_directory_alt(char *capt, char *root);
EXPORTED_FUNCTION double get_color(double defcol);
EXPORTED_FUNCTION double get_color_ext(double defcol, char *title);
EXPORTED_FUNCTION char *widget_get_caption();
EXPORTED_FUNCTION double widget_set_caption(char *str);
EXPORTED_FUNCTION void *widget_get_owner();
EXPORTED_FUNCTION double widget_set_owner(void *hwnd);
EXPORTED_FUNCTION char *widget_get_icon();
EXPORTED_FUNCTION double widget_set_icon(char *icon);
EXPORTED_FUNCTION char *widget_get_system();
EXPORTED_FUNCTION double widget_set_system(char *sys);

double show_message(char *str) {
  return dialog_module::show_message(str);
}

double show_message_cancelable(char *str) {
  return dialog_module::show_message_cancelable(str);
}

double show_question(char *str) {
  return dialog_module::show_question(str);
}

double show_question_cancelable(char *str) {
  return dialog_module::show_question_cancelable(str);
}

double show_attempt(char *str) {
  return dialog_module::show_attempt(str);
}

double show_error(char *str, double abort) {
  return dialog_module::show_error(str, abort);
}

char *get_string(char *str, char *def) {
  return dialog_module::get_string(str, def);
}

char *get_password(char *str, char *def) {
  return dialog_module::get_password(str, def);
}

double get_integer(char *str, double def) {
  return dialog_module::get_integer(str, def);
}

double get_passcode(char *str, double def) {
  return dialog_module::get_passcode(str, def);
}

char *get_open_filename(char *filter, char *fname) {
  return dialog_module::get_open_filename(filter, fname);
}

char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
  return dialog_module::get_open_filename_ext(filter, fname, dir, title);
}

char *get_open_filenames(char *filter, char *fname) {
  return dialog_module::get_open_filenames(filter, fname);
}

char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
  return dialog_module::get_open_filenames_ext(filter, fname, dir, title);
}

char *get_save_filename(char *filter, char *fname) {
  return dialog_module::get_save_filename(filter, fname);
}

char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
  return dialog_module::get_save_filename_ext(filter, fname, dir, title);
}

char *get_directory(char *dname) {
  return dialog_module::get_directory(dname);
}

char *get_directory_alt(char *capt, char *root) {
  return dialog_module::get_directory_alt(capt, root);
}

double get_color(double defcol) {
  return dialog_module::get_color((int)defcol);
}

double get_color_ext(double defcol, char *title) {
  return dialog_module::get_color_ext((int)defcol, title);
}

char *widget_get_caption() {
  return dialog_module::widget_get_caption();
}

double widget_set_caption(char *str) {
  dialog_module::widget_set_caption(str);
  return 0;
}

char *widget_get_icon() {
  return dialog_module::widget_get_icon();
}

double widget_set_icon(char *icon) {
  dialog_module::widget_set_icon(icon);
  return 0;
}

void *widget_get_owner() {
  return dialog_module::widget_get_owner();
}

double widget_set_owner(void *hwnd) {
  dialog_module::widget_set_owner(hwnd);
  return 0;
}

char *widget_get_system() {
  return dialog_module::widget_get_system();
}

double widget_set_system(char *sys) {
  dialog_module::widget_set_system(sys);
  return 0;
}
