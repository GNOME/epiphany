# Embedded PDF Viewer based on pdf.js

This directory contains an official pdf.js release version, distributed at: https://github.com/mozilla/pdf.js

## Update process
1. Delete existing files except epiphany-pdfjs-changes.patch, pdfjs_generate_resource.py, and README.epiphany.md.
2. Grab a new official release (*-dist.zip) distributed at github and extract everything into this directory.
3. Delete precompiled .map files (viewer.js.map, pdf.js.map, pdf.worker.js.map, pdf.sandbox.js.map).
4. Create WIP git commit to help with updating our patch in step 6.
5. Manually merge changes from epiphany-pdfjs-changes.patch into web/viewer.[html/js/css].
6. Recreate epiphany-pdfjs-changes.patch: `git diff > tmp && mv tmp epiphany-pdfjs-changes.patch`
7. Recreate the resource xml file with `./pdfjs_generate_resource.py`

## Modifications to viewer.html
1. In order to circumvent CORS the pdf is downloaded and then embedded into the viewer.html file. Therefore the head has an
extra attribute 'pdf_data="%s' which will be replace by Epiphany. Extracting this data is done in viewer.js.

2. Futhermore 'pdf_name="%s"' is supplied to set the download file name.

3. '<base href="ephy-resource:///org/gnome/epiphany/pdfjs/web/">' has been added to the head section.

## Modifications to viewer.js

function webViewerInitialized() has been changed:

1. Set file to ''

2. Replace the end of the function with:

  var file_name = document.head.getAttribute('pdf_name')
  var raw = atob(document.head.getAttribute('pdf_data'));
  var raw_length = raw.length;
  var array = new Uint8Array(new ArrayBuffer(raw_length));

  for(var i = 0; i < raw_length; i++) {
    array[i] = raw.charCodeAt(i);
  }
    
  try {
      PDFViewerApplication.open(array);
      PDFViewerApplication.setTitleUsingUrl(file_name);
  } catch (reason) {
    PDFViewerApplication.l10n.get('loading_error', null, 'An error occurred while loading the PDF.').then(function (msg) {
      PDFViewerApplication.error(msg, reason);
    });
  }

3. Disable history modification, otherwise base url is set as main url and translation won't work.

In _pushOrReplaceState early return before the window.history.xxx changes

4. Hide secondaryViewBookmark and viewBookmark
Switch id's secondaryViewBookmark and viewBookmark from visibleSmallView and hiddenSmallView to hidden.

## Modifications to viewer.css

Here are two small changes for webkit specific layout:

1. Adding:
-webkit-appearance: none; to .dropdownToolbarButton > select {

2. Adding:
.toolbarField.pageNumber { blocks in the css


## Patch with changes for Epiphany

For easier updating to a newer version there is a patch file `epiphany-pdfjs-changes.patch` containing the necessary changes to the viewer files.

## Note
Do not add map files to this bundle (webinspector will complain about it), but they are not needed here and would increase our storage size otherwise.

# Documentation created by Jan-Michael Brummer <jan.brummer@tabos.org>


