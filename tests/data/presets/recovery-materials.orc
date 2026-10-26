<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): recovery devices whose materials are of other types than
     OpenRocket's recovery devices use, between two body tubes: a streamer with a BULK sheet, a
     parachute with a BULK line material and one with a BULK canopy. OpenRocket loads all five
     presets; QtRocket loads the body tubes and skips the others with a warning each. -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Components>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BT-20</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <InsideDiameter Unit="in">0.710</InsideDiameter>
            <OutsideDiameter Unit="in">0.736</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <Streamer>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>ST-1</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <Length Unit="in">36</Length>
            <Width Unit="in">2</Width>
            <Thickness Unit="in">0.001</Thickness>
        </Streamer>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>PK-1</PartNumber>
            <Material Type="SURFACE">Ripstop nylon</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12</LineLength>
            <LineMaterial Type="BULK">Cardboard</LineMaterial>
        </Parachute>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>PK-2</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12</LineLength>
            <LineMaterial Type="LINE">Elastic cord (round 2 mm, 1/16 in)</LineMaterial>
        </Parachute>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BT-50</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <InsideDiameter Unit="in">0.950</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
    </Components>
</OpenRocketComponent>
