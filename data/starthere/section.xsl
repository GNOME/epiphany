<?xml version="1.0"?>
<xsl:stylesheet
        xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
        version="1.0">

<xsl:template match="/">
   <xsl:apply-templates/>
</xsl:template>

<xsl:template match="section">
<html>
  <head>
  <style type="text/css">
    body {
	    margin-left: 150;
	    margin-right: 150;
	    background: <xsl:value-of select="@bg"/>;
	    color:      <xsl:value-of select="@text"/>;
	    font-family: Verdana, Helvetica, Arial, sans-serif; 
	    font-size: 14px; 
    }

    div.topbar {
	    height: 25px; 
	    padding: 7px; 
	    color: <xsl:value-of select="@title"/>;
	    background-color: <xsl:value-of select="@title-bg"/>;
	    font-weight: bold;
	    margin-bottom: 50;
    }

    div.topbar a {
	    margin-right: 15;
	    font-size: 20px;
	    font-weight: bold;
	    color: <xsl:value-of select="@title"/>;
    }

    h2 {
	    color: <xsl:value-of select="@headings"/>;
	    font-size: 18px;
    }

    a {
	    color: <xsl:value-of select="@link"/>
    }

    a.visited {
	    color: <xsl:value-of select="@link"/>
    }
  </style>
  </head>
  <body>
    <xsl:apply-templates/>
  </body>
</html>
</xsl:template>

<xsl:template match="content">
<div class="body">
  <xsl:apply-templates/>
</div>
</xsl:template>

<xsl:template match="bar">
<div class="topbar">
  <xsl:apply-templates/>
</div>
</xsl:template>

<xsl:template match="item">
<a href="{@section}">
  <xsl:apply-templates/>
</a>
</xsl:template>

<xsl:template match="title">
<h2>
  <xsl:apply-templates/>
</h2>
</xsl:template>

<xsl:template match="paragraph">
<p>
  <xsl:apply-templates/>
</p>
</xsl:template>

<xsl:template match="buildcontent">
<p>
  <xsl:apply-templates/>
</p>
</xsl:template>

<xsl:template match="action">
<a href="start-here:{@id}?{@param}">
  <xsl:apply-templates/>
</a><br/>
</xsl:template>

<xsl:template match="smartbookmark">
<a type="text/smartbookmark" href="{@normal}" rel="{@smart}" title="{@title}">
  <xsl:apply-templates/>
</a><br/>
</xsl:template>

</xsl:stylesheet>

