<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): a second <Materials> replaces the first, but JAXB reads the
     first one all the same, and the unknown unit of measure of its material fails the file. -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Materials>
        <Material UnitsOfMeasure="lb/in3">
            <Name>Lead</Name>
            <Density>0.41</Density>
            <Type>BULK</Type>
        </Material>
    </Materials>
    <Materials>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Cardboard</Name>
            <Density>680.0</Density>
            <Type>BULK</Type>
        </Material>
    </Materials>
    <Components>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BT-20</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <InsideDiameter Unit="in">0.710</InsideDiameter>
            <OutsideDiameter Unit="in">0.736</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
    </Components>
</OpenRocketComponent>
