<?xml version="1.0"?>
<xsl:stylesheet
        xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
        version="1.0">

<!-- root rule -->
<xsl:template match="/">
   <xsl:apply-templates/>
</xsl:template>

<!-- main rule for document element -->
<xsl:template match="section">
<html>
  <head>
  </head>
  <body>
    <xsl:apply-templates/>
  </body>
</html>
</xsl:template>

</xsl:stylesheet>

