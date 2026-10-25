<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): how a component's <Material> and a parachute's
     <LineMaterial> find their material (BaseComponentDTO.find()). -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Materials>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Cardboard</Name>
            <Density>680.0</Density>
            <Type>BULK</Type>
        </Material>
        <Material UnitsOfMeasure="g/cm3">
            <Name>Paper test</Name>
            <Density>0.8</Density>
            <Type>BULK</Type>
        </Material>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Balsa, bulk</Name>
            <Density>128.0</Density>
            <Type>BULK</Type>
            <ShearModulus>2.5e8</ShearModulus>
            <Group>Woods</Group>
        </Material>
        <Material>
            <Name>Basswood, bulk</Name>
            <Density>500.0</Density>
            <Type>BULK</Type>
        </Material>
    </Materials>
    <Components>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>section</PartNumber>
            <Material Type="BULK">Cardboard</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>section g/cm3</PartNumber>
            <Material Type="BULK">Paper test</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>section shear and group</PartNumber>
            <Material Type="BULK">Balsa, bulk</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>section SI</PartNumber>
            <Material Type="BULK">Basswood, bulk</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>built-in ignoring case</PartNumber>
            <Material Type="BULK">cardboard</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>other type</PartNumber>
            <Material Type="SURFACE">Paper test</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>new in a group</PartNumber>
            <Material Type="BULK" Group="Woods">Unobtainium</Material>
            <InsideDiameter Unit="in">0.95</InsideDiameter>
            <OutsideDiameter Unit="in">0.976</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>ThreadsLines elastic</PartNumber>
            <Material Type="SURFACE">Ripstop nylon</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12</LineLength>
            <LineMaterial Type="LINE" Group="ThreadsLines">Elastic cord (round 2 mm, 1/16 in)</LineMaterial>
        </Parachute>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>ThreadsLines other</PartNumber>
            <Material Type="SURFACE">Ripstop nylon</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12</LineLength>
            <LineMaterial Type="LINE" Group="ThreadsLines">Thread (heavy-duty)</LineMaterial>
        </Parachute>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>ThreadsLines unknown</PartNumber>
            <Material Type="SURFACE">Ripstop nylon</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12</LineLength>
            <LineMaterial Type="LINE" Group="ThreadsLines">Mystery cord</LineMaterial>
        </Parachute>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>Kevlars unknown</PartNumber>
            <Material Type="SURFACE">Ripstop nylon</Material>
            <Diameter Unit="in">12</Diameter>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12</LineLength>
            <LineMaterial Type="LINE" Group="Kevlars">Mystery cord</LineMaterial>
        </Parachute>
    </Components>
</OpenRocketComponent>
