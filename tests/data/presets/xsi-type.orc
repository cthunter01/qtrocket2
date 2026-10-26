<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): a body tube whose element names another DTO class with
     xsi:type. OpenRocket's JAXB makes a centering ring of it; QtRocket ignores xsi:type and makes
     a body tube. -->
<OpenRocketComponent xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    <Version>0.1</Version>
    <Components>
        <BodyTube xsi:type="centeringRingDTO">
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>RA-2050</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <InsideDiameter Unit="in">0.736</InsideDiameter>
            <OutsideDiameter Unit="in">0.950</OutsideDiameter>
            <Length Unit="in">0.0625</Length>
        </BodyTube>
    </Components>
</OpenRocketComponent>
