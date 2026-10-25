<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): a legacy file; a streamer puts its base properties before
     the Legacy flag. -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Legacy/>
    <Materials>
        <Material UnitsOfMeasure="kg/m2">
            <Name>Mylar, 1 mil</Name>
            <Density>0.0355</Density>
            <Type>SURFACE</Type>
        </Material>
    </Materials>
    <Components>
        <Streamer>
            <Manufacturer>Quest</Manufacturer>
            <PartNumber>ST-1</PartNumber>
            <Description>Streamer, mylar, 1 in. x 30 in.</Description>
            <Material Type="SURFACE">Mylar, 1 mil</Material>
            <Length Unit="in">30.0</Length>
            <Width Unit="in">1.0</Width>
            <Thickness Unit="in">0.001</Thickness>
        </Streamer>
        <BulkHead>
            <Manufacturer>Quest</Manufacturer>
            <PartNumber>BH-1 </PartNumber>
            <Description>Bulkhead	plywood</Description>
            <Material Type="BULK">Plywood (birch)</Material>
            <OutsideDiameter Unit="in">0.95</OutsideDiameter>
            <Length Unit="in">0.125</Length>
            <Mass Unit="oz">0.1</Mass>
        </BulkHead>
    </Components>
</OpenRocketComponent>
