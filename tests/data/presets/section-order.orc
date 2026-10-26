<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): the lookup of a <Material> stops at the first section
     material that matches, so the material of no known type behind it is never asked for its
     type; the first <Components>, whose body tube would come to it, is replaced by the second
     and never read; and the unknown <FinSet> is no component. -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Materials>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Cardboard</Name>
            <Density>680.0</Density>
            <Type>BULK</Type>
        </Material>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Mystery</Name>
            <Density>1.0</Density>
            <Type>ROCK</Type>
        </Material>
    </Materials>
    <Components>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>replaced</PartNumber>
            <Material Type="BULK">Mystery</Material>
        </BodyTube>
    </Components>
    <Components>
        <FinSet>
            <Manufacturer>Estes</Manufacturer>
        </FinSet>
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
