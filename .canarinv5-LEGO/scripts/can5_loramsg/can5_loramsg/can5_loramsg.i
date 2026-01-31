%module can5_loramsg
%include "exception.i"     
%include "stdint.i"
%include "carrays.i"
%include "cdata.i"
%include "typemaps.i"

extern int can5_lmsg_ntoh_packet(struct can5_lmsg_packet *packet, int len);
extern void can5_lmsg_hton_packet(struct can5_lmsg_packet *packet);

extern int can5_lmsg_ntoh_packet_ack(struct can5_lmsg_packet *packet, int len);
extern void can5_lmsg_hton_packet_ack(struct can5_lmsg_packet *packet);

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
#include "can5_loramsg.h"
%}

%apply (char *STRING, size_t LENGTH) { (const char* data, int len) }

%newobject can5_lmsg_bin_to_packet;
%newobject can5_lmsg_make_ack_packet;
%newobject can5_lmsg_make_packet;

%begin %{
#define SWIG_PYTHON_STRICT_BYTE_CHAR
%}

%include "can5_loramsg.h"

