SRC = plan.xml
TARGET = plan.html
XSLT = plan2table.xsl

all: validate transform

transform:
	xsltproc --output $(TARGET) $(XSLT) $(SRC)

validate:
	xmllint --noout --valid $(SRC)

clean:
	rm -f $(TARGET)
