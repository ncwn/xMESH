%module canarin_loramsg
%include "exception.i"     
%include "stdint.i"
%include "carrays.i"
%include "cdata.i"
%include "typemaps.i"

extern int can5_ntoh_s_packet(struct can5_s_packet *packet, int len);
extern void can5_hton_s_packet(struct can5_s_packet *packet);

extern int can5_ntoh_s_packet_ack(struct can5_s_packet *packet, int len);
extern void can5_hton_s_packet_ack(struct can5_s_packet *packet);

%except {
  try {
    $action
  } catch(RangeError) {
    SWIG_exception(SWIG_ValueError, "Range Error");
  } catch(DivisionByZero) {
    SWIG_exception(SWIG_DivisionByZero, "Division by zero");
  } catch(OutOfMemory) {
    SWIG_exception(SWIG_MemoryError, "Out of memory");
  } catch(...) {
    SWIG_exception(SWIG_RuntimeError, "Unknown exception");
  }
}

%array_class(int, intArray);
%{
#include "canarin_loramsg.h"

struct can5_s_packet *

struct can5_s_packet *

%}

%apply (char *STRING, size_t LENGTH) { (const char* data, int len) }

%newobject bin_to_s_packet;
%newobject make_ack_packet;
%newobject make_packet;

%begin %{
#define SWIG_PYTHON_STRICT_BYTE_CHAR
%}

%include "canarin_loramsg.h"

