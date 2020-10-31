//===- cfplist.c - Main c implementation file for cfplist ext ---*-  C  -*-===//
//
// This source file is part of the cfplist open source project.
//
// Copyright (c) 2020 J. Morgan Lieberthal and the cfplist authors
// Licensed under Apache License, Version 2.0
//
//===----------------------------------------------------------------------===//

#include <CoreFoundation/CoreFoundation.h>

#include "ruby.h"
#include "ruby/intern.h"
#include "ruby/ruby.h"

/*******************************************************************************
 *                                   Macros                                    *
 *******************************************************************************/

#define CF2RB(X) corefoundation_to_ruby((X))
#define CFSTR2RB(STR) rb_CFString_convert((STR))
#define CFDATA2RB(D) rb_CFData_convert((D))
#define CFDATE2RB(D) rb_CFDate_convert((D))
#define CFBOOL2RB(B) rb_CFBoolean_convert((B))
#define CFNUM2RB(N) rb_CFNumber_convert((N))
#define CFARR2RB(A) rb_CFArray_convert((A))
#define CFDICT2RB(D) rb_CFDictionary_convert((D))

#define CFSTR2SYM(STR) rb_to_symbol(rb_CFString_convert((STR)))
#define CFINTERN(X) rb_CFString_intern((X))

#define cfcheckmem(PTR, M, ...)                                                \
  do {                                                                         \
    if ((PTR) == NULL) {                                                       \
      rb_raise(rb_eNoMemError, M##__VA_ARGS__);                                \
    }                                                                          \
  } while (0)

#define IS_DOMAIN(S, D) (CFStringCompare((S), (D), 0) == kCFCompareEqualTo)

/*******************************************************************************
 *                                Declarations                                 *
 *******************************************************************************/

static VALUE
corefoundation_to_ruby(CFTypeRef cf_type);

static CFTypeRef
ruby_to_corefoundation(VALUE obj);

static void
rb_raise_CFError(CFErrorRef error);

VALUE rb_mCFPlist;
VALUE rb_eCFError;
VALUE rb_eCFErrorOSStatus;
VALUE rb_eCFErrorMach;
VALUE rb_eCFErrorCocoa;
static ID id_to_s, id_keys, id_vals, id_count;

/*******************************************************************************
 *                     CoreFoundation Type => Ruby Object                      *
 *******************************************************************************/

/*
 * Rather than calling CFStringGetCStringPtr here, we want to actually copy
 * the memory of the string represented by the CFStringRef. This is because
 * these CF functions we are calling follow the "Get Rule", meaning that we
 * don't "own" these objects, and their memory could be freed by
 * CoreFoundation at any time.
 *
 * Even though the function we call hash 'Get' in the name, it does
 * actually perform a copy of the string, according to the documentation.
 */
static inline VALUE
rb_CFString_convert(CFStringRef str)
{
  CFRetain(str); /* retain the string for the duration of this function */

  CFIndex len = CFStringGetLength(str); /* get length of string */
  CFIndex buflen = len + 1; /* size of our char buffer, plus NULL terminator */

  /*
   * Allocate a char buffer to hold the string. Size is 1+len to account for the
   * null terminator. sizeof(char) is 1, but I feel this more accurately depicts
   * what we are doing.
   */
  char *cstr = calloc((1 + len), sizeof(char));

  Boolean success;
  /* get the characters from the string, using UTF8 encoding. */
  success = CFStringGetCString(str, cstr, buflen, kCFStringEncodingUTF8);

  if (!success) {
    /* TODO: Raise a better error here. */
    rb_raise(rb_eRuntimeError, "Unable to convert string to UTF8");
  }

  CFRelease(str); /* matches retain from the beginning of this function. */

  /* Let ruby know this string is UTF8 encoded. */
  return rb_utf8_str_new(cstr, (long)len);
}

/* This isn't ideal, but since Ruby handles unknown byte sequences as a string,
 * we will too. To make this readable, you'll likely have to call String#unpack
 * on the result.
 *
 * TODO: Figure out a safe way to Marshal this data
 */
static inline VALUE
rb_CFData_convert(CFDataRef data)
{
  CFRetain(data); /* retain the data for the duration of this function */

  CFIndex len = CFDataGetLength(data); /* get length of data, in bytes */
  CFRange rng = CFRangeMake(0, len);   /* range of bytes to copy (i.e. all) */

  /* allocate our data buffer on the heap, unlike the analogous CFString
   * function, we don't need to account for a NUL terminator
   */
  uint8_t *databuf = calloc(len, sizeof(uint8_t)); /* yes, I know it's 1... */
  cfcheckmem(databuf, "Unable to allocate data buffer for CFData\n");

  /* this function doesn't return anything, so no real error checking */
  CFDataGetBytes(data, rng, databuf); /* copy the bytes */

  CFRelease(data); /* matches retain above */
  /* return a tainted string, because this data could be literally anything */
  return rb_tainted_str_new((const char *)databuf, len);
}

/* why is CFBoolean a thing? I think this one is pretty self-explanatory. */
static inline VALUE
rb_CFBoolean_convert(CFBooleanRef boolean)
{
  Boolean val = CFBooleanGetValue(boolean);
  if (val) {
    return Qtrue;
  }
  return Qfalse;
}

/*
 * Convert a CFNumber into a ruby number.
 */
static inline VALUE
rb_CFNumber_convert(CFNumberRef number)
{
  CFRetain(number); /* retain for the duration of this function */
  intptr_t rawval;  /* get a value big enough to hold a pointer to an int */
  CFNumberType num_type = CFNumberGetType(number); /* what kind of number? */
  Boolean success = CFNumberGetValue(number, num_type, &rawval);
  VALUE result = Qnil; /* assume it will be nil, in case we can't convert */

  if (!success) {
    CFRelease(number);
    return result; /* TODO: raise here? */
  }

  switch (num_type) {
  case kCFNumberSInt8Type:     /* char */
  case kCFNumberCharType:      /* char */
    result = LONG2FIX(rawval); /* smaller than 32-bit, so use FIXNUM */
    break;

  case kCFNumberShortType:    /* short */
  case kCFNumberSInt16Type:   /* short */
  case kCFNumberSInt32Type:   /* int */
  case kCFNumberIntType:      /* int */
    result = INT2FIX(rawval); /* smaller than 32-bit, so use FIXNUM */
    break;

  case kCFNumberLongType:      /* long */
  case kCFNumberNSIntegerType: /* long */
  case kCFNumberCFIndexType:   /* signed long */
    result = LONG2NUM(rawval); /* maybe 64-bit, so use Numeric */
    break;

  case kCFNumberSInt64Type:   /* long long */
  case kCFNumberLongLongType: /* long long */
    result = LL2NUM(rawval);  /* maybe 64-bit, so use Numeric */
    break;

  case kCFNumberFloatType:    /* float */
  case kCFNumberDoubleType:   /* double */
  case kCFNumberFloat64Type:  /* 64-bit floating point (MacTypes.h)*/
  case kCFNumberFloat32Type:  /* 32-bit floating point (MacTypes.h)*/
  case kCFNumberCGFloatType:  /* float or double */
    result = DBL2NUM(rawval); /* maybe 64-bit, so use Numeric */
    break;
  }

  CFRelease(number); /* matches retain above */
  return result;
}

VALUE
rb_CFArray_convert(CFArrayRef array)
{
  CFRetain(array); /* retain for the duration of this function */
  CFIndex i, count = CFArrayGetCount(array); /* number of elements in array */

  /* faster to allocate the memory for our ruby array in one shot */
  VALUE result = rb_ary_new_capa(count);

  /* loop over the array, and coerce the types to ruby types */
  for (i = 0; i < count; i++) {
    CFTypeRef val = CFArrayGetValueAtIndex(array, i);
    /* NOTE: this macro copies the memory of the values */
    rb_ary_push(result, CF2RB(val));
  }

  CFRelease(array); /* matches retain above */

  return result;
}

VALUE
rb_CFDictionary_convert(CFDictionaryRef dict, bool keys2sym)
{
  CFRetain(dict);                                /* retain for the duration */
  VALUE result = rb_hash_new();                  /* create a new hash */
  CFIndex i, count = CFDictionaryGetCount(dict); /* count of y<->v pairs */

  /* Since we are getting these keys from a Property List, they are
   * guaranteed to be CFStrings, and we are safe to allocate this 'array'
   * of
   * CFStringRefs. If they weren't guaranteed to be CFStrings, we would
   * have
   * to do something else.
   */
  CFStringRef *keys = calloc(count, sizeof(CFStringRef));

  /* The only guarantee about the values our CFDictionaryRef contains
   * is that they will be one of the following:
   *     - CFData
   *     - CFString
   *     - CFArray
   *     - CFDictionary
   *     - CFDate
   *     - CFBoolean
   *     - CFNumber
   * Since we won't know beforehand, we need to allocate an 'array' of
   * CFTypeRefs to hold them.
   */
  CFTypeRef *values = calloc(count, sizeof(CFTypeRef));

  /* Get the keys and values from the CFDictionary. Note that ownership
   * follows the "Get Rule", so we will need to copy the values during
   * their
   * own conversion functions.
   */
  CFDictionaryGetKeysAndValues(dict, (const void **)keys,
                               (const void **)values);
  /* loop through keys and values, convert to ruby objects, and add them
   * to
   * our hash
   */
  for (i = 0; i < count; i++) {
    if (keys2sym) { /* convert the CFStringRefs to Symbols */
      rb_hash_aset(result, CFSTR2SYM(keys[i]), CF2RB(values[i]));
    } else { /* just use plain strings as the keys */
      rb_hash_aset(result, CFSTR2RB(keys[i]), CF2RB(values[i]));
    }
  }

  CFRelease(dict); /* matches retain above */
  return result;
}

static inline VALUE
rb_CFDate_convert(CFDateRef date)
{
  /* WHY IS THERE NO CFDateGetTimeIntervalSince1970 ??? */
  /* This function returns a representation a specific point in time
   * relative to the "absolute reference date of 1 Jan 2001 00:00:00 GMT."
   *
   * How arbitrary...
   */
  CFAbsoluteTime abstime = CFDateGetAbsoluteTime(date);
  /* add seconds between epoch and our 'reference date' to the time
   * interval,
   * so we can get the time interval in seconds since epoch
   */
  CFTimeInterval since_epoch = abstime + kCFAbsoluteTimeIntervalSince1970;
  return rb_time_new(since_epoch, 0); /* return the time object */
}

static VALUE
corefoundation_to_ruby(CFTypeRef cf_type)
{
  /*
   *     - CFData       <=>
   *     - CFString     <=> String
   *     - CFArray      <=> Array
   *     - CFDictionary <=> Hash
   *     - CFDate       <=> Date/Time
   *     - CFBoolean    <=> TrueClass, FalseClass
   *     - CFNumber     <=> Numeric
   */
  if (!cf_type)
    return Qnil; /* nil if NULL */

  CFTypeID tid = CFGetTypeID(cf_type);
  if (tid == CFDataGetTypeID()) {
    return rb_CFData_convert(cf_type);
  } else if (tid == CFStringGetTypeID()) {
    return rb_CFString_convert(cf_type);
  } else if (tid == CFArrayGetTypeID()) {
    return rb_CFArray_convert(cf_type);
  } else if (tid == CFDictionaryGetTypeID()) {
    return rb_CFDictionary_convert(cf_type, false);
  } else if (tid == CFDateGetTypeID()) {
    return rb_CFDate_convert(cf_type);
  } else if (tid == CFBooleanGetTypeID()) {
    return rb_CFBoolean_convert(cf_type);
  } else if (tid == CFNumberGetTypeID()) {
    return rb_CFNumber_convert(cf_type);
  } else {
    return Qnil;
  }
}

/*******************************************************************************
 *                     Ruby Object => CoreFoundation Type                      *
 *******************************************************************************/

/**
 * Convert a ruby string to a CFStringRef
 */
static CFStringRef
cf_rstring_convert(VALUE obj)
{
  StringValue(obj);
  CFIndex len;
  CFStringRef result;

  /* get the length of the ruby string */
  len = RSTRING_LEN(obj);
  /* allocate a uint8_t buffer for the result string */
  const uint8_t *cstr = (uint8_t *)RSTRING_PTR(obj);

  result = CFStringCreateWithBytes(kCFAllocatorDefault, cstr, len,
                                   kCFStringEncodingUTF8, true);
  return result;
}

/**
 * Convert a ruby object to a CFTypeRef.
 *
 * We really should figure out how to convert this to data or Marshal it, but
 * that's too complicated right now.
 *
 * TODO: Figure out how to Marshal this, or how to convert it to data.
 */
static CFTypeRef
cf_robject_convert(VALUE obj)
{
  if (rb_respond_to(obj, id_to_s)) {
    return cf_rstring_convert(obj);
  }

  return NULL; /* not ideal */
}

/**
 * Convert a Ruby Hash to a CFDictionaryRef
 */
static CFDictionaryRef
cf_rhash_convert(VALUE obj)
{
  /*
   * This is where we will build the CFDictionary.
   * It is Mutable because we want to be able to set keys and values.
   */
  CFMutableDictionaryRef tmp_result;
  /* This is the final immutable result. */
  CFDictionaryRef result;
  tmp_result = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

  /*
   * These are the keys and values of the ruby hash we want to convert.
   */
  VALUE keys, vals;
  keys = rb_funcall(obj, id_keys, 0);
  vals = rb_funcall(obj, id_vals, 0);

  CFIndex i, count = NUM2LONG(rb_hash_size(obj));

  /* These are used in the below loop. */
  VALUE rb_k, rb_v;
  CFTypeRef cf_k, cf_v;

  for (i = 0; i < count; i++) {
    rb_k = rb_ary_entry(keys, i);
    rb_v = rb_ary_entry(vals, i);

    cf_k = ruby_to_corefoundation(rb_k);
    cf_v = ruby_to_corefoundation(rb_v);

    CFDictionarySetValue(tmp_result, cf_k, cf_v);
  }

  result = CFDictionaryCreateCopy(kCFAllocatorDefault, tmp_result);
  CFRelease(tmp_result); /* Matches initial create */
  return result;
}

/**
 * Convert a Ruby Array to a CFArrayRef.
 */
static CFArrayRef
cf_rarray_convert(VALUE obj)
{
  Check_Type(obj, T_ARRAY);
  VALUE countv = rb_funcall(obj, id_count, 0); /* obj.count */
  CFIndex i, count = NUM2LONG(countv);

  CFMutableArrayRef tmp_result;
  CFArrayRef result;

  tmp_result =
      CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);

  for (i = 0; i < count; i++) {
    VALUE rval = rb_ary_entry(obj, i);
    CFTypeRef cfval = ruby_to_corefoundation(rval);
    CFArraySetValueAtIndex(tmp_result, i, &cfval);
  }

  result = CFArrayCreateCopy(kCFAllocatorDefault, tmp_result);
  CFRelease(tmp_result); /* matches initial create */
  return result;
}

/**
 * Convert a CFBooleanRef to ruby.
 */
static CFBooleanRef
cf_rbool_convert(VALUE obj)
{
  if (obj == Qtrue) {
    return kCFBooleanTrue;
  } else {
    return kCFBooleanFalse;
  }
}

/**
 * Convert a Ruby Numeric type to a CFNumberRef.
 */
static CFNumberRef
cf_rnumeric_convert(VALUE obj)
{
  CFNumberRef result;

  switch (TYPE(obj)) {
  case T_FLOAT:
  case T_RATIONAL:
  case T_COMPLEX: {
    double val = NUM2DBL(obj);
    result = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &val);
    break;
  }
  default: {
    CFIndex val = NUM2LONG(obj); /* 64-bit for overflow safety */
    result = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &val);
    break;
  }
  }

  return result;
}

/**
 * Convert any ruby type to a CFTypeRef.
 */
static CFTypeRef
ruby_to_corefoundation(VALUE obj)
{
  switch (TYPE(obj)) {
  case T_STRING:
    return cf_rstring_convert(obj);
  case T_ARRAY:
    return cf_rarray_convert(obj);
  case T_HASH:
    return cf_rhash_convert(obj);
  case T_TRUE:
  case T_FALSE:
    return cf_rbool_convert(obj);
  case T_FLOAT:
  case T_BIGNUM:
  case T_FIXNUM:
    return cf_rnumeric_convert(obj);

  default:
    return cf_robject_convert(obj);
  }
}

/**
 * Convert a ruby object to a CFPropertyListRef.
 */
static inline VALUE
cfplist_to_ruby(CFPropertyListRef plist, Boolean symbolize_keys)
{
  CFTypeID plist_type = CFGetTypeID(plist);
  if (plist_type == CFArrayGetTypeID()) {
    return rb_CFArray_convert(plist);
  } else if (plist_type == CFDictionaryGetTypeID()) {
    return rb_CFDictionary_convert(plist, symbolize_keys);
  }

  /* Return nil if it's not an array or hash */
  return Qnil;
}

/*******************************************************************************
 *                              Ruby Method Defs                               *
 *******************************************************************************/

/**
 * Raises a CFError, casting to the appropriate ruby type
 */
static void
rb_raise_CFError(CFErrorRef error)
{
  CFStringRef domain = CFErrorGetDomain(error);
  CFIndex code = CFErrorGetCode(error);

  /* These next two calls use the Copy/Create rule, so we must release the refs
   * when we are done. */
  CFStringRef cf_desc = CFErrorCopyDescription(error);
  CFStringRef cf_reason = CFErrorCopyFailureReason(error);

  /* It's okay to get pointers for these strings, since we "own" them. */
  const char *desc = CFStringGetCStringPtr(cf_desc, kCFStringEncodingUTF8);
  const char *reason = CFStringGetCStringPtr(cf_reason, kCFStringEncodingUTF8);

  if (IS_DOMAIN(domain, kCFErrorDomainPOSIX)) {
    /* we have a POSIX error. We raise these as Ruby SysErrors */
    VALUE msg = rb_sprintf("%s %s", desc, reason);
    /* at this point, we know "code" is an "errno", so we cast to int */
    VALUE exc = rb_syserr_new_str((int)code, msg);
    rb_exc_raise(exc);
  } else if (IS_DOMAIN(domain, kCFErrorDomainOSStatus)) {
    rb_raise(rb_eCFErrorOSStatus, "%s %s (%ld)", desc, reason, code);
  } else if (IS_DOMAIN(domain, kCFErrorDomainMach)) {
    rb_raise(rb_eCFErrorMach, "Mach Error - %s %s (%ld)", desc, reason, code);
  } else if (IS_DOMAIN(domain, kCFErrorDomainCocoa)) {
    rb_raise(rb_eCFErrorCocoa, "%s %s (%ld)", desc, reason, code);
  } else {
    /* This is probably unreachable */
    rb_raise(rb_eCFError, "%s %s (%ld)", desc, reason, code);
  }
}

/**
 * Parses a string representation of a PList to a ruby hash.
 *
 * TODO: Add more complex options
 */
static VALUE
plist_parse(int argc, VALUE *argv, VALUE self)
{
  VALUE plist_str, v_symbolize_keys;
  Boolean symbolize_keys;

  if (1 == rb_scan_args(argc, argv, "11", &plist_str, &v_symbolize_keys)) {
    v_symbolize_keys = Qfalse;
  }

  if (NIL_P(v_symbolize_keys) || v_symbolize_keys == Qfalse) {
    symbolize_keys = false;
  } else {
    symbolize_keys = true;
  }

  StringValue(plist_str);

  /* allocate a buffer to hold the string data */
  const uint8_t *strdata = (const uint8_t *)StringValuePtr(plist_str);
  CFIndex strlen = RSTRING_LEN(plist_str);

  /* Create a CFDataRef with the data from the string */
  CFDataRef plist_data;
  plist_data = CFDataCreate(kCFAllocatorDefault, strdata, strlen);

  CFErrorRef err = NULL;
  CFPropertyListRef plist;

  /* create the plist from the data */
  plist = CFPropertyListCreateWithData(kCFAllocatorDefault, plist_data,
                                       kCFPropertyListImmutable, NULL, &err);

  /* check to make sure no error occured */
  if (err != NULL) {
    /* if an error did occur, clean up our references */
    if (plist != NULL)
      CFRelease(plist); /* matches initial create */
    if (plist_data != NULL)
      CFRelease(plist_data); /* matches initial create */
    rb_raise_CFError(err);
  }

  /* Convert the CFPropertyListRef to a ruby object */
  VALUE result = cfplist_to_ruby(plist, symbolize_keys);

  /* clean up our references */
  if (plist != NULL)
    CFRelease(plist);
  if (plist_data != NULL)
    CFRelease(plist_data);
  if (err != NULL)
    CFRelease(err);

  return result;
}

void
Init_cfplist(void)
{
  id_to_s = rb_intern("to_s");
  id_keys = rb_intern("keys");
  id_vals = rb_intern("values");
  id_count = rb_intern("count");

  rb_mCFPlist = rb_define_module("CFPlist");
  rb_eCFError =
      rb_define_class_under(rb_mCFPlist, "CFError", rb_eStandardError);
  rb_eCFErrorOSStatus =
      rb_define_class_under(rb_mCFPlist, "OSStatusError", rb_eCFError);
  rb_eCFErrorMach =
      rb_define_class_under(rb_mCFPlist, "MachError", rb_eCFError);
  rb_eCFErrorCocoa =
      rb_define_class_under(rb_mCFPlist, "CocoaError", rb_eCFError);

  rb_define_module_function(rb_mCFPlist, "_parse", plist_parse, -1);
}
