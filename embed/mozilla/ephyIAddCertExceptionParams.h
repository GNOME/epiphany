/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM ephyIAddCertExceptionParams.idl
 */

#ifndef __gen_ephyIAddCertExceptionParams_h__
#define __gen_ephyIAddCertExceptionParams_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    ephyIAddCertExceptionParams */
#define EPHYIADDCERTEXCEPTIONPARAMS_IID_STR "2bac2eb2-cd10-4e3f-802e-dade8096d6c5"

#define EPHYIADDCERTEXCEPTIONPARAMS_IID \
  {0x2bac2eb2, 0xcd10, 0x4e3f, \
    { 0x80, 0x2e, 0xda, 0xde, 0x80, 0x96, 0xd6, 0xc5 }}

class NS_NO_VTABLE NS_SCRIPTABLE ephyIAddCertExceptionParams : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(EPHYIADDCERTEXCEPTIONPARAMS_IID)

  /* readonly attribute boolean prefetchCert; */
  NS_SCRIPTABLE NS_IMETHOD GetPrefetchCert(PRBool *aPrefetchCert) = 0;

  /* readonly attribute AString location; */
  NS_SCRIPTABLE NS_IMETHOD GetLocation(nsAString & aLocation) = 0;

  /* attribute boolean exceptionAdded; */
  NS_SCRIPTABLE NS_IMETHOD GetExceptionAdded(PRBool *aExceptionAdded) = 0;
  NS_SCRIPTABLE NS_IMETHOD SetExceptionAdded(PRBool aExceptionAdded) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(ephyIAddCertExceptionParams, EPHYIADDCERTEXCEPTIONPARAMS_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_EPHYIADDCERTEXCEPTIONPARAMS \
  NS_SCRIPTABLE NS_IMETHOD GetPrefetchCert(PRBool *aPrefetchCert); \
  NS_SCRIPTABLE NS_IMETHOD GetLocation(nsAString & aLocation); \
  NS_SCRIPTABLE NS_IMETHOD GetExceptionAdded(PRBool *aExceptionAdded); \
  NS_SCRIPTABLE NS_IMETHOD SetExceptionAdded(PRBool aExceptionAdded); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_EPHYIADDCERTEXCEPTIONPARAMS(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetPrefetchCert(PRBool *aPrefetchCert) { return _to GetPrefetchCert(aPrefetchCert); } \
  NS_SCRIPTABLE NS_IMETHOD GetLocation(nsAString & aLocation) { return _to GetLocation(aLocation); } \
  NS_SCRIPTABLE NS_IMETHOD GetExceptionAdded(PRBool *aExceptionAdded) { return _to GetExceptionAdded(aExceptionAdded); } \
  NS_SCRIPTABLE NS_IMETHOD SetExceptionAdded(PRBool aExceptionAdded) { return _to SetExceptionAdded(aExceptionAdded); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_EPHYIADDCERTEXCEPTIONPARAMS(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetPrefetchCert(PRBool *aPrefetchCert) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetPrefetchCert(aPrefetchCert); } \
  NS_SCRIPTABLE NS_IMETHOD GetLocation(nsAString & aLocation) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetLocation(aLocation); } \
  NS_SCRIPTABLE NS_IMETHOD GetExceptionAdded(PRBool *aExceptionAdded) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetExceptionAdded(aExceptionAdded); } \
  NS_SCRIPTABLE NS_IMETHOD SetExceptionAdded(PRBool aExceptionAdded) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetExceptionAdded(aExceptionAdded); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public ephyIAddCertExceptionParams
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_EPHYIADDCERTEXCEPTIONPARAMS

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, ephyIAddCertExceptionParams)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* readonly attribute boolean prefetchCert; */
NS_IMETHODIMP _MYCLASS_::GetPrefetchCert(PRBool *aPrefetchCert)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AString location; */
NS_IMETHODIMP _MYCLASS_::GetLocation(nsAString & aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean exceptionAdded; */
NS_IMETHODIMP _MYCLASS_::GetExceptionAdded(PRBool *aExceptionAdded)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP _MYCLASS_::SetExceptionAdded(PRBool aExceptionAdded)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_ephyIAddCertExceptionParams_h__ */
