<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" encoding="UTF-8" indent="yes" />

<xsl:template match="/plan">
    <html><head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
        <style type="text/css">
            table { background-color: #EEF; margin: 0 0.5em 1em 0.5em; }
            .blank { padding: 0.5em 0; }
            .title { background-color: #669; color: #FFF; font-size: 1.2em; font-weight: bold; padding: 0 0.25em; text-align: left; }
            .description {background-color: #CAA; padding: 0.25em 0.5em; text-align: left; }
            .bugzilla a { color: #FFF; text-decoration: underline; }
            .responsible { white-space: nowrap; }
            .status { padding: 0.25em 1em; text-align: center; }
            .completed { background-color: #0F0; color: #FFF; }
            .delayed { background-color: #909; color: #FFF; }
            .designed { background-color: #F60; color: #FFF; }
            .implementing { background-color: #FF0; color: #FFF; }
            .orphan { background-color: #00F; color: #FFF; }
            .pending { background-color: #F00; color: #FFF; }
        </style>
    </head><body>
        <h1>Epiphany <xsl:value-of select="@version" /> Plan</h1>
<table>
        <xsl:apply-templates select="item">
            <!-- Sort by status first, then sort by title (both alphabetically)-->
            <xsl:sort select="status/@id" />
            <xsl:sort select="title" />
        </xsl:apply-templates>
</table>
    </body></html>
</xsl:template>

<xsl:template match="item">
    <tr>
        <td class="title" colspan="2">
            <xsl:value-of select="title" />
            <span class="bugzilla">
                <xsl:if test="normalize-space(bugzilla/@id)">
                    (<a href="http://bugzilla.gnome.org/show_bug.cgi?id={bugzilla/@id}">#<xsl:value-of select="bugzilla/@id" /></a>)
                </xsl:if>
            </span>
        </td>
    </tr>
    <tr>
        <td class="description"><xsl:value-of select="description" />
        <span class="responsible">
            <xsl:if test="normalize-space(responsible)">
                    [<a href="mailto:{responsible/@email}"><xsl:value-of select="responsible" /></a>]
            </xsl:if>
        </span>
        </td>
        <td class="status"><span class="{status/@id}"><xsl:value-of select="status/@id" /></span></td>
    </tr>
    <tr>
        <td class="blank" colspan="2"></td>
    </tr>
</xsl:template>

</xsl:stylesheet>
