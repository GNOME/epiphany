/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM ephyAddCertException.idl
 */

#ifndef __gen_ephyAddCertException_h__
#define __gen_ephyAddCertException_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

#ifndef __gen_nsIDOMWindow_h__
#include "nsIDOMWindow.h"
#endif

#ifndef __gen_nsIDOMDocument_h__
#include "nsIDOMDocument.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    ephyAddCertException */
#define EPHYADDCERTEXCEPTION_IID_STR "cfdca027-c2c7-446a-8031-4d0041ab9f1e"

#define EPHYADDCERTEXCEPTION_IID \
  {0xcfdca027, 0xc2c7, 0x446a, \
    { 0x80, 0x31, 0x4d, 0x00, 0x41, 0xab, 0x9f, 0x1e }}

class NS_NO_VTABLE ephyAddCertException : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(EPHYADDCERTEXCEPTION_IID)

  /* void showAddCertExceptionDialog (in nsIDOMDocument aDocument); */
  NS_IMETHOD ShowAddCertExceptionDialog(nsIDOMDocument *aDocument) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(ephyAddCertException, EPHYADDCERTEXCEPTION_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_EPHYADDCERTEXCEPTION \
  NS_IMETHOD ShowAddCertExceptionDialog(nsIDOMDocument *aDocument); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_EPHYADDCERTEXCEPTION(_to) \
  NS_IMETHOD ShowAddCertExceptionDialog(nsIDOMDocument *aDocument) { return _to ShowAddCertExceptionDialog(aDocument); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_EPHYADDCERTEXCEPTION(_to) \
  NS_IMETHOD ShowAddCertExceptionDialog(nsIDOMDocument *aDocument) { return !_to ? NS_ERROR_NULL_POINTER : _to->ShowAddCertExceptionDialog(aDocument); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class _MYCLASS_ : public ephyAddCertException
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_EPHYADDCERTEXCEPTION

  _MYCLASS_();

private:
  ~_MYCLASS_();

protected:
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(_MYCLASS_, ephyAddCertException)

_MYCLASS_::_MYCLASS_()
{
  /* member initializers and constructor code */
}

_MYCLASS_::~_MYCLASS_()
{
  /* destructor code */
}

/* void showAddCertExceptionDialog (in nsIDOMDocument aDocument); */
NS_IMETHODIMP _MYCLASS_::ShowAddCertExceptionDialog(nsIDOMDocument *aDocument)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_ephyAddCertException_h__ */
