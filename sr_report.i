// vim : set fileencoding=utf-8 expandtab noai ts=4 sw=4 :
/// @addtogroup sr_report
/// @{
/// @file sr_report.i
/// 
/// @date 2013-2014
/// @copyright All rights reserved.
///            Any reproduction, use, distribution or disclosure of this
///            program, without the express, prior written consent of the 
///            authors is strictly prohibited.
/// @author Rolf Meyer
%module sr_report

%include "usi.i"
%include "std_string.i"

%{
USI_REGISTER_MODULE(sr_report);
%}

namespace sc_core {
enum sc_severity {
    SC_INFO = 0,        // informative only
    SC_WARNING, // indicates potentially incorrect condition
    SC_ERROR,   // indicates a definite problem
    SC_FATAL,   // indicates a problem from which we cannot recover
    SC_MAX_SEVERITY
};
}

void set_filter_to_whitelist(bool value);
void add_sc_object_to_filter(sc_core::sc_object *obj, sc_core::sc_severity severity, int verbosity);
void remove_sc_object_from_filter(sc_core::sc_object *obj);


%{
#include "core/common/sr_report/sr_report.h"

void set_filter_to_whitelist(bool value) {
  sr_report_handler::set_filter_to_whitelist(value);
}

void add_sc_object_to_filter(sc_core::sc_object *obj, sc_severity severity, int verbosity) {
  if(obj) {
    sr_report_handler::add_sc_object_to_filter(obj, severity, verbosity);
  }
}

void remove_sc_object_from_filter(sc_core::sc_object *obj) {
  if(obj) {
    sr_report_handler::remove_sc_object_from_filter(obj);
  }
}

/*
std::vector<sc_core::sc_object *> show_sc_object_in_filter() {
  sr_report_handler::show_sc_object_in_filter();
}
*/

%}


