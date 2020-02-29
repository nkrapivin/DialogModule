/*
 
 MIT License
 
 Copyright © 2020 Samuel Venable
 
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
#include <thread>

#ifdef _WIN32
#define EXPORTED_FUNCTION extern "C" __declspec(dllexport)
#else /* macOS, Linux, and BSD */
#define EXPORTED_FUNCTION extern "C" __attribute__((visibility("default")))
#endif

namespace {

unsigned dialog_identifier = 100;
void(*CreateAsynEventWithDSMap)(int, int);
int(*CreateDsMap)(int _num, ...);
bool(*DsMapAddDouble)(int _index, char *_pKey, double value);
bool(*DsMapAddString)(int _index, char *_pKey, char *pVal);

void threaded_double_result_helper(double id, double func) {
  double result = func;
  int resultMap = CreateDsMap(0);
  DsMapAddDouble(resultMap, (char *)"id", id);
  DsMapAddDouble(resultMap, (char *)"status", 1);
  DsMapAddDouble(resultMap, (char *)"result", result);
  CreateAsynEventWithDSMap(resultMap, 70);
}

void threaded_string_result_helper(double id, char *func) {
  char *result = func;
  int resultMap = CreateDsMap(0);
  DsMapAddDouble(resultMap, (char *)"id", id);
  DsMapAddDouble(resultMap, (char *)"status", 1);
  DsMapAddString(resultMap, (char *)"result", result);
  CreateAsynEventWithDSMap(resultMap, 70);
}

void show_message_threaded(char *str, unsigned id) {
  threaded_double_result_helper(id, dialog_module::show_message(str));
}

void show_message_cancelable_threaded(char *str, unsigned id) {
  threaded_double_result_helper(id, dialog_module::show_message_cancelable(str));
}

void show_question_threaded(char *str, unsigned id) {
  threaded_double_result_helper(id, dialog_module::show_question(str));
}

void show_question_cancelable_threaded(char *str, unsigned id) {
  threaded_double_result_helper(id, dialog_module::show_question_cancelable(str));
}

void show_attempt_threaded(char *str, unsigned id) {
  threaded_double_result_helper(id, dialog_module::show_attempt(str));
}

void show_error_threaded(char *str, double abort, unsigned id) {
  threaded_double_result_helper(id, dialog_module::show_error(str, abort));
}

void get_string_threaded(char *str, char *def, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_string(str, def));
}

void get_password_threaded(char *str, char *def, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_password(str, def));
}

void get_integer_threaded(char *str, double def, unsigned id) {
  threaded_double_result_helper(id, dialog_module::get_integer(str, def));
}

void get_passcode_threaded(char *str, double def, unsigned id) {
  threaded_double_result_helper(id, dialog_module::get_passcode(str, def));
}

void get_open_filename_threaded(char *filter, char *fname, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_open_filename(filter, fname));
}

void get_open_filename_ext_threaded(char *filter, char *fname, char *dir, char *title, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_open_filename_ext(filter, fname, dir, title));
}

void get_open_filenames_threaded(char *filter, char *fname, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_open_filenames(filter, fname));
}

void get_open_filenames_ext_threaded(char *filter, char *fname, char *dir, char *title, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_open_filenames_ext(filter, fname, dir, title));
}

void get_save_filename_threaded(char *filter, char *fname, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_save_filename(filter, fname));
}

void get_save_filename_ext_threaded(char *filter, char *fname, char *dir, char *title, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_save_filename_ext(filter, fname, dir, title));
}

void get_directory_threaded(char *dname, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_directory(dname));
}

void get_directory_alt_threaded(char *capt, char *root, unsigned id) {
  threaded_string_result_helper(id, dialog_module::get_directory_alt(capt, root));
}

void get_color_threaded(double defcol, unsigned id) {
  threaded_double_result_helper(id, dialog_module::get_color((int)defcol));
}

void get_color_ext_threaded(double defcol, char *title, unsigned id) {
  threaded_double_result_helper(id, dialog_module::get_color_ext((int)defcol, title));
}

} // anonymous namespace

EXPORTED_FUNCTION double show_message(char *str);
EXPORTED_FUNCTION double show_message_async(char *str);
EXPORTED_FUNCTION double show_message_cancelable(char *str);
EXPORTED_FUNCTION double show_message_cancelable_async(char *str);
EXPORTED_FUNCTION double show_question(char *str);
EXPORTED_FUNCTION double show_question_async(char *str);
EXPORTED_FUNCTION double show_question_cancelable(char *str);
EXPORTED_FUNCTION double show_question_cancelable_async(char *str);
EXPORTED_FUNCTION double show_attempt(char *str);
EXPORTED_FUNCTION double show_attempt_async(char *str);
EXPORTED_FUNCTION double show_error(char *str, double abort);
EXPORTED_FUNCTION double show_error_async(char *str, double abort);
EXPORTED_FUNCTION char *get_string(char *str, char *def);
EXPORTED_FUNCTION double get_string_async(char *str, char *def);
EXPORTED_FUNCTION char *get_password(char *str, char *def);
EXPORTED_FUNCTION double get_password_async(char *str, char *def);
EXPORTED_FUNCTION double get_integer(char *str, double def);
EXPORTED_FUNCTION double get_integer_async(char *str, double def);
EXPORTED_FUNCTION double get_passcode(char *str, double def);
EXPORTED_FUNCTION double get_passcode_async(char *str, double def);
EXPORTED_FUNCTION char *get_open_filename(char *filter, char *fname);
EXPORTED_FUNCTION double get_open_filename_async(char *filter, char *fname);
EXPORTED_FUNCTION char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION double get_open_filename_ext_async(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION char *get_open_filenames(char *filter, char *fname);
EXPORTED_FUNCTION double get_open_filenames_async(char *filter, char *fname);
EXPORTED_FUNCTION char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION double get_open_filenames_ext_async(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION char *get_save_filename(char *filter, char *fname);
EXPORTED_FUNCTION double get_save_filename_async(char *filter, char *fname);
EXPORTED_FUNCTION char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION double get_save_filename_ext_async(char *filter, char *fname, char *dir, char *title);
EXPORTED_FUNCTION char *get_directory(char *dname);
EXPORTED_FUNCTION double get_directory_async(char *dname);
EXPORTED_FUNCTION char *get_directory_alt(char *capt, char *root);
EXPORTED_FUNCTION double get_directory_alt_async(char *capt, char *root);
EXPORTED_FUNCTION double get_color(double defcol);
EXPORTED_FUNCTION double get_color_async(double defcol);
EXPORTED_FUNCTION double get_color_ext(double defcol, char *title);
EXPORTED_FUNCTION double get_color_ext_async(double defcol, char *title);
EXPORTED_FUNCTION char *widget_get_caption();
EXPORTED_FUNCTION double widget_set_caption(char *str);
EXPORTED_FUNCTION void *widget_get_owner();
EXPORTED_FUNCTION double widget_set_owner(void *hwnd);
EXPORTED_FUNCTION char *widget_get_icon();
EXPORTED_FUNCTION double widget_set_icon(char *icon);
EXPORTED_FUNCTION char *widget_get_system();
EXPORTED_FUNCTION double widget_set_system(char *sys);
EXPORTED_FUNCTION void RegisterCallbacks(char *arg1, char *arg2, char *arg3, char *arg4);

double show_message(char *str) {
  return dialog_module::show_message(str);
}

double show_message_async(char *str) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(show_message_threaded, str, id);
  dialog_thread.detach();
  return (double)id;
}

double show_message_cancelable(char *str) {
  return dialog_module::show_message_cancelable(str);
}

double show_message_cancelable_async(char *str) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(show_message_cancelable_threaded, str, id);
  dialog_thread.detach();
  return (double)id;
}

double show_question(char *str) {
  return dialog_module::show_question(str);
}

double show_question_async(char *str) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(show_question_threaded, str, id);
  dialog_thread.detach();
  return (double)id;
}

double show_question_cancelable(char *str) {
  return dialog_module::show_question_cancelable(str);
}

double show_question_cancelable_async(char *str) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(show_question_cancelable_threaded, str, id);
  dialog_thread.detach();
  return (double)id;
}

double show_attempt(char *str) {
  return dialog_module::show_attempt(str);
}

double show_attempt_async(char *str) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(show_attempt_threaded, str, id);
  dialog_thread.detach();
  return (double)id;
}

double show_error(char *str, double abort) {
  return dialog_module::show_error(str, abort);
}

double show_error_async(char *str, double abort) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(show_error_threaded, str, abort, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_string(char *str, char *def) {
  return dialog_module::get_string(str, def);
}

double get_string_async(char *str, char *def) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_string_threaded, str, def, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_password(char *str, char *def) {
  return dialog_module::get_password(str, def);
}

double get_password_async(char *str, char *def) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_password_threaded, str, def, id);
  dialog_thread.detach();
  return (double)id;
}

double get_integer(char *str, double def) {
  return dialog_module::get_integer(str, def);
}

double get_integer_async(char *str, double def) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_integer_threaded, str, def, id);
  dialog_thread.detach();
  return (double)id;
}

double get_passcode(char *str, double def) {
  return dialog_module::get_passcode(str, def);
}

double get_passcode_async(char *str, double def) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_passcode_threaded, str, def, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_open_filename(char *filter, char *fname) {
  return dialog_module::get_open_filename(filter, fname);
}

double get_open_filename_async(char *filter, char *fname) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_open_filename_threaded, filter, fname, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
  return dialog_module::get_open_filename_ext(filter, fname, dir, title);
}

double get_open_filename_ext_async(char *filter, char *fname, char *dir, char *title) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_open_filename_ext_threaded, filter, fname, dir, title, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_open_filenames(char *filter, char *fname) {
  return dialog_module::get_open_filenames(filter, fname);
}

double get_open_filenames_async(char *filter, char *fname) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_open_filenames_threaded, filter, fname, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
  return dialog_module::get_open_filenames_ext(filter, fname, dir, title);
}

double get_open_filenames_ext_async(char *filter, char *fname, char *dir, char *title) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_open_filenames_ext_threaded, filter, fname, dir, title, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_save_filename(char *filter, char *fname) {
  return dialog_module::get_save_filename(filter, fname);
}

double get_save_filename_async(char *filter, char *fname) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_save_filename_threaded, filter, fname, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
  return dialog_module::get_save_filename_ext(filter, fname, dir, title);
}

double get_save_filename_ext_async(char *filter, char *fname, char *dir, char *title) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_save_filename_ext_threaded, filter, fname, dir, title, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_directory(char *dname) {
  return dialog_module::get_directory(dname);
}

double get_directory_async(char *dname) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_directory_threaded, dname, id);
  dialog_thread.detach();
  return (double)id;
}

char *get_directory_alt(char *capt, char *root) {
  return dialog_module::get_directory_alt(capt, root);
}

double get_directory_alt_async(char *capt, char *root) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_directory_alt_threaded, capt, root, id);
  dialog_thread.detach();
  return (double)id;
}

double get_color(double defcol) {
  return dialog_module::get_color((int)defcol);
}

double get_color_async(double defcol) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_color_threaded, (int)defcol, id);
  dialog_thread.detach();
  return (double)id;
}

double get_color_ext(double defcol, char *title) {
  return dialog_module::get_color_ext((int)defcol, title);
}

double get_color_ext_async(double defcol, char *title) {
  unsigned id = dialog_identifier++;
  std::thread dialog_thread(get_color_ext_threaded, (int)defcol, title, id);
  dialog_thread.detach();
  return (double)id;
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

void RegisterCallbacks(char *arg1, char *arg2, char *arg3, char *arg4) {
  void(*CreateAsynEventWithDSMapPtr)(int, int) = (void(*)(int, int))(arg1);
  int(*CreateDsMapPtr)(int _num, ...) = (int(*)(int _num, ...))(arg2);
  CreateAsynEventWithDSMap = CreateAsynEventWithDSMapPtr;
  CreateDsMap = CreateDsMapPtr;

  bool(*DsMapAddDoublePtr)(int _index, char *_pKey, double value) = (bool(*)(int, char *, double))(arg3);
  bool(*DsMapAddStringPtr)(int _index, char *_pKey, char *pVal) = (bool(*)(int, char *, char *))(arg4);

  DsMapAddDouble = DsMapAddDoublePtr;
  DsMapAddString = DsMapAddStringPtr;
}
