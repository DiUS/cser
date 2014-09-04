# Cser - Code generator for C struct serialization

Most languages these days provide some sort of automatic serialization
support, be it within the language itself (e.g. via introspection) or through
common frameworks. For some reason C has failed to attract something useful
in this area, and this is what Cser attempts to address.

With Cser you give it a C source file, and tell it to generate serialization
and deserialization functions for one or more types, to one or more formats.
It then parses the C source, works out all the relevant type dependencies,
and then generates the necessary (de)serialization functions for you.


# Supported data types and structures

The primary use case for Cser is for internal state persisting and restoring.
As such, it is able to handle most of the common data structures, as well as
the basic data types (such as ints). Specifically, the following constructs
are understood by Cser:

* Structs
* Arrays
* Indirect object (i.e. pointer to something)
* Single-linked list
* Zero-terminated list (including C strings and pointer arrays)
* Variable-length array member in a struct, provided an earlier member
  in the same struct contains the length.
* Unions, by way of tagging one of the member variables.

* Double-linked lists can be handled by omitting the back reference and using
  a post-processing step to reestablish them after restoring.

Annotations are done using C pragma statements, either via the
single-line `#pragma foo` or, for better readability, the in-line
`_Pragma("foo")` (which the pre-processor rewrites anyway).


# Supported backend formats

Currently only two backend formats are supported - binary and XML, but Cser
has been designed to make it easy to add further backends. While XML is
largely an interchange format, data interchange is not the main purpose of
Cser, and because of this the level of control over the resulting XML schema
is quite limited.


## Binary / raw

The binary backend provides a simple, compact serialization format which is
neither wasteful nor optimized. No special encoding or compression is
performed, but on the other hand no overhead is added to the data, other
than a flag marking whether a pointer is null or not.

The I/O interface is simple and highly flexible. Whenever a structure is
(de)serialized, it also takes a function pointer which is responsible for
sinking or sourcing the serialized bytes.


## XML

The XML backend serializes data to/from basic XML schemas. Data is
sunk/sourced in a streaming fashion (as opposed to document). These
sink/source functions must be provided by the application, and would
typically be wrappers around whatever XML library is in use. As such
the I/O interface is quite simple, but not quite as flexible as the
binary/raw I/O interface.

As a quick overview, the I/O interface comprises the following prototypes:

    extern bool cser_xml_opentag (const cser_xml_tag_t *tag, void *ctx);
    extern bool cser_xml_setvalue (const char *value, void *ctx);
    extern bool cser_xml_closetag (const char *tagname, void *ctx);
    extern bool cser_xml_nexttag (cser_xml_tag_t *tag, void *ctx);
    extern char *cser_xml_getvalue (void *ctx);


# Example

To demonstrate most of the constructs supported by Cser, consider a
header file containing the type definitions for which we want serializers:

    #include <stdint.h>
    
    typedef struct example
    {
      int16_t a;
      int16_t *b;
      uint32_t c[3];
      char *str;
      unsigned long num_bytes;
      uint8_t *bytes _Pragma("cser varlen:num_bytes");
    } example_t;
    
    typedef struct node
    {
      union {
        uint64_t all _Pragma("cser select");
        struct {
          uint8_t byte[8];
        } each;
      };
      struct node *next;
    } node_t;

The example_t type shows off plain data, pointer-to-data, array of data,
zero-terminated array (default assumption for char*), as well as a
variable-length array. Note the use of an in-line pragma to link the
`bytes` pointer to the `num_bytes` member.

In the small linked list type node_t we see how a union member can be
serialized, by specifying which aspect to use during serialization.

To generate binary serialization support for these types, feed in the
pre-processed source to Cser like so:

    $ cc -E example.h | ./cser -o example_serializers -b raw -i example.h -v example_t node_t

    typedef signed char int8_t;
    typedef short int int16_t;
    typedef int int32_t;
    typedef long int int64_t;
    typedef unsigned char uint8_t;
    typedef unsigned short int uint16_t;
    typedef unsigned int uint32_t;
    typedef unsigned long int uint64_t;
    typedef signed char int_least8_t;
    typedef short int int_least16_t;
    typedef int int_least32_t;
    typedef long int int_least64_t;
    typedef unsigned char uint_least8_t;
    typedef unsigned short int uint_least16_t;
    typedef unsigned int uint_least32_t;
    typedef unsigned long int uint_least64_t;
    typedef signed char int_fast8_t;
    typedef long int int_fast16_t;
    typedef long int int_fast32_t;
    typedef long int int_fast64_t;
    typedef unsigned char uint_fast8_t;
    typedef unsigned long int uint_fast16_t;
    typedef unsigned long int uint_fast32_t;
    typedef unsigned long int uint_fast64_t;
    typedef long int intptr_t;
    typedef unsigned long int uintptr_t;
    typedef long int intmax_t;
    typedef unsigned long int uintmax_t;
    typedef struct {
      short int a;
      short int* b;
      unsigned int[3] c;
      char* /*zeroterm*/ str;
      unsigned long num_bytes;
      unsigned char* /*varlen:num_bytes*/ bytes;
    } struct example;

    typedef struct example example_t;
    warning: ignoring unsupported member on line 114
    warning: ignoring unsupported member on line 115
    typedef struct {
      unsigned long int all;
      struct node* next;
    } struct node;

    warning: ignoring unmentionable struct/union on line 117
    typedef struct node node_t;
    $

Here we ran with -v to have Cser list the type definitions it builds up,
together with relevant annotations - this can be very useful for verifying
that it's doing what you want it to do.

You might also note a few warnings being printed. In this case they are
harmless, though annoying, referring to the unused union members. Fixing these
warnings would be nice - patches are welcome!

The resulting code has been placed in example_serializers.h and
example_serializers.c. Inspecting the header file we find the prototypes
and type definitions, with some comments:

    ...
    /* The callback functions take a buffer, a length, and an opaque */
    /* pointer which is passed through. They MUST return zero (0) on */
    /* success. Any non-zero value is treated as an error and bubbled*/
    /* back up to the caller. Note that "short" reads and writes   */
    /* are NOT used or supported in this interface, unlike that of   */
    /* read(2)/write(2).                                             */
    typedef int (*cser_raw_write_fn) (const uint8_t *bytes, size_t n, void *q);
    typedef int (*cser_raw_read_fn) (uint8_t *bytes, size_t n, void *q);
    
    int cser_raw_store_struct_node (const struct node *val, cser_raw_write_fn w, void *q);
    int cser_raw_load_struct_node (struct node *val, cser_raw_read_fn r, void *q);
    int cser_raw_store_struct_example (const struct example *val, cser_raw_write_fn w, void *q);
    int cser_raw_load_struct_example (struct example *val, cser_raw_read_fn r, void *q);
    ...

Once you have implemented sink and source functions matching the
`cser_raw_write_fn` and `cser_raw_read_fn` prototypes, you can start
serializing and deserializing:

    node_t *my_list = ...
    if (!cser_raw_store_struct_node (my_list, writer_fn, out_file))
    {
      whinge ("serialization failed");
      ...
    }

    node_t *loaded_list = 0;
    if (!cser_raw_load_struct_node (loaded_list, reader_fn, in_file))
    {
      whinge ("deserialization failed");
      ...
    }


# Cser pragmas

- *zeroterm*
  Mark a pointer member as being a zero-terminated array.
  This is the default for `char *` members.

- *single*
  Mark a pointer member as only pointing to a single element.
  This is only needed to override the default on `char *` members.

- *varlen:[num_member]*
  Mark a pointer member as being of variable length, with the number of
  items (not bytes!) available in the `num_member` member. Said member
  *MUST* be listed before the pointer member in the struct, as Cser does
  not at this point support clever reordering.

- *select*
  Specifies the aspect of a union member to use for serialization. By
  default unions are not serializable as there is no way for Cser to know
  which aspect will be valid at the point of serialization. Using this
  pragma allows the user to tell Cser that he/she knows better. Use
  with caution.

- *omit*
  Instructs Cser to ignore the marked member altogether. The storage
  for it will be zero-initialized on load. This feature is useful if
  the persisted construct also contains cached data which is not needed
  or appropriate to persist.

- *emit*
  The inverse of `omit`. Currently no use case is known, but it seemed
  appropriate (and trivial) to implement together with `omit`.


# Command line options

- *-o [basename]*
  Sets the basename of the output files. The default is 'out'.

- *-b [backend]*
  Specifies the backend to use (e.g. 'xml', 'raw'). The default is 'raw'.
  Multiple backends may be specified using multible -b options.

- *-i [header]*
  Force 'header' to be `#include'd` in the generated header file.
  Typically the same header file which is used as input will be listed
  here to make the generated code free-standing.

- *-v*
  Verbose mode. Causes Cser to print the type definitions as it processes
  them, complete with annotations. Useful for troubleshooting.

- *-h*
  Shows the help.

- *[typename...]*
  The typenames to generate serialization support for.


# FAQ

- Does Cser provide automatic versioning?

  No, at this stage Cser does not provide automatic versioning. It is
  however quite straight forward to achieve backwards compatibility manually
  by versioning the struct definitions themselves though.

- Pragmas are ugly, can I use macros?

  Yes. The only tricky one is the `varlen` macro, but it can be handled
  using this construct:

    # define _MKSTRb(x)      #x
    # define _MKSTR(x)      _MKSTRb(x)
    # define VARLEN(x)      _Pragma(_MKSTR(cser varlen:x))

  You can then use it like this:

    struct {
      unsigned num;
      int *items VARLEN(num);
    } foo_t;

- What if I want my binary serialization to include different fields than my
  XML serialization of the same type?

  The C pre-processor has you covered here too. As an example:

    #ifdef SERGEN_XML
    # define XML_OMIT           _Pragma("cser omit")
    # define RAW_OMIT           
    #elif SERGEN_RAW
    # define XML_OMIT
    # define RAW_OMIT           _Pragma("cser omit")
    #else
    # define XML_OMIT
    # define RAW_OMIT
    #endif

    struct {
      int a     XML_OMIT;
      signed b  RAW_OMIT;
    } bar_t;

  Then simply pre-process with -DSERGEN_XML or -DSERGEN_RAW to get the
  Cser pragmas applied where you want them.


# Known issues

- Some of the warnings produced are unwarranted and spurious.

- Union support is limited and not fully tested.

- Function pointers can confuse the parser and cause errors.

- Including system headers frequently leads to lots of warnings due to the
  weird and wonderful approaches commonly found in such headers.

- The XML backend has one potential memory leak in one of the failure paths.

