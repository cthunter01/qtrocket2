<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): presets OpenRocket loads and QtRocket skips with a warning:
     a parachute without a LineCount, a length that is not finite and a material whose density is
     not finite. The first body tube loads in both. -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Materials>
        <Material UnitsOfMeasure="g/cm3">
            <Name>Neutronium</Name>
            <Density>1e306</Density>
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
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>PK-12</PartNumber>
            <Material Type="SURFACE">Ripstop nylon</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>six</LineCount>
            <LineLength Unit="in">12</LineLength>
        </Parachute>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BT-INF</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <InsideDiameter Unit="in">0.710</InsideDiameter>
            <OutsideDiameter Unit="in">0.736</OutsideDiameter>
            <Length Unit="in">INF</Length>
        </BodyTube>
        <BulkHead>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BH-N</PartNumber>
            <Material Type="BULK">Neutronium</Material>
            <OutsideDiameter Unit="in">0.95</OutsideDiameter>
            <Length Unit="in">0.125</Length>
        </BulkHead>
    </Components>
</OpenRocketComponent>
