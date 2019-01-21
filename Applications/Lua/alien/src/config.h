#define  HAVE_FFI_H        1
#define  FFI_CLOSURES      1

#if FFI_CLOSURES

#ifdef _MSC_VER
__declspec(align(8))
#endif
typedef struct {
  char tramp[FFI_TRAMPOLINE_SIZE];
  ffi_cif   *cif;
  void     (*fun)(ffi_cif*,void*,void**,void*);
  void      *user_data;
#ifdef __GNUC__
} ffi_closure __attribute__((aligned (8)));
#else
} ffi_closure;
#endif

void *ffi_closure_alloc (size_t size, void **code);
void ffi_closure_free (void *);

ffi_status
ffi_prep_closure (ffi_closure*,
		  ffi_cif *,
		  void (*fun)(ffi_cif*,void*,void**,void*),
		  void *user_data);

ffi_status
ffi_prep_closure_loc (ffi_closure*,
		      ffi_cif *,
		      void (*fun)(ffi_cif*,void*,void**,void*),
		      void *user_data,
		      void*codeloc);

typedef struct {
  char tramp[FFI_TRAMPOLINE_SIZE];

  ffi_cif   *cif;

#if !FFI_NATIVE_RAW_API

  /* if this is enabled, then a raw closure has the same layout
     as a regular closure.  We use this to install an intermediate
     handler to do the transaltion, void** -> ffi_raw*. */

  void     (*translate_args)(ffi_cif*,void*,void**,void*);
  void      *this_closure;

#endif

  void     (*fun)(ffi_cif*,void*,ffi_raw*,void*);
  void      *user_data;

} ffi_raw_closure;


