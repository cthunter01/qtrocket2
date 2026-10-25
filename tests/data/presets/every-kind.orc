<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<!-- QtRocket test fixture (README.md): one component of each of the eleven kinds of an .orc file,
     with the optional elements of each, laid out as OpenRocket's bundled files are. -->
<OpenRocketComponent>
    <Version>0.1</Version>
    <Materials>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Paper, spiral kraft glassine, bulk</Name>
            <Density>799.0</Density>
            <Type>BULK</Type>
        </Material>
        <Material UnitsOfMeasure="g/cm3">
            <Name>Polystyrene, cast, bulk</Name>
            <Density>1.05</Density>
            <Type>BULK</Type>
        </Material>
        <Material UnitsOfMeasure="g/m2">
            <Name>Nylon fabric, ripstop, 1.7 oz actual</Name>
            <Density>67.0</Density>
            <Type>SURFACE</Type>
        </Material>
        <Material UnitsOfMeasure="kg/m">
            <Name>Nylon Paracord, 110 lb, 1/16 in. dia.</Name>
            <Density>0.00097</Density>
            <Type>LINE</Type>
        </Material>
        <Material UnitsOfMeasure="kg/m3">
            <Name>Balsa, bulk</Name>
            <Density>128.0</Density>
            <Type>BULK</Type>
            <ShearModulus>2.5e8</ShearModulus>
            <Group>Woods</Group>
        </Material>
    </Materials>
    <Components>
        <BodyTube>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BT-20</PartNumber>
            <Description>Body tube, BT-20, 18 in.</Description>
            <Material Type="BULK">Paper, spiral kraft glassine, bulk</Material>
            <InsideDiameter Unit="in">0.710</InsideDiameter>
            <OutsideDiameter Unit="in">0.736</OutsideDiameter>
            <Length Unit="in">18.0</Length>
        </BodyTube>
        <TubeCoupler>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>JT-20C</PartNumber>
            <Description>Tube coupler, BT-20</Description>
            <Material Type="BULK">Paper, spiral kraft glassine, bulk</Material>
            <InsideDiameter Unit="mm">17.0</InsideDiameter>
            <OutsideDiameter Unit="mm">18.0</OutsideDiameter>
            <Length Unit="cm">2.5</Length>
            <Mass Unit="g">1.2</Mass>
        </TubeCoupler>
        <NoseCone>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>PNC-20A</PartNumber>
            <Description>Nose cone, plastic, BT-20</Description>
            <Material Type="BULK">Polystyrene, cast, bulk</Material>
            <Mass Unit="oz">0.3</Mass>
            <Filled>false</Filled>
            <Shape>OGIVE</Shape>
            <OutsideDiameter Unit="in">0.736</OutsideDiameter>
            <ShoulderDiameter Unit="in">0.708</ShoulderDiameter>
            <ShoulderLength Unit="in">0.75</ShoulderLength>
            <Length Unit="in">2.75</Length>
            <Thickness Unit="in">0.062</Thickness>
        </NoseCone>
        <Transition>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>TA-2050</PartNumber>
            <Description>Transition, BT-20 to BT-50, balsa</Description>
            <Material Type="BULK">Balsa, bulk</Material>
            <Filled>true</Filled>
            <Shape>CONICAL</Shape>
            <ForeOutsideDiameter Unit="in">0.736</ForeOutsideDiameter>
            <ForeShoulderDiameter Unit="in">0.710</ForeShoulderDiameter>
            <ForeShoulderLength Unit="in">0.5</ForeShoulderLength>
            <AftOutsideDiameter Unit="in">0.976</AftOutsideDiameter>
            <AftShoulderDiameter Unit="in">0.950</AftShoulderDiameter>
            <AftShoulderLength Unit="in">0.5</AftShoulderLength>
            <Length Unit="in">1.25</Length>
        </Transition>
        <BulkHead>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>BHB-20</PartNumber>
            <Description>Bulkhead, balsa, BT-20</Description>
            <Material Type="BULK">balsa</Material>
            <OutsideDiameter Unit="in/64">0.703</OutsideDiameter>
            <Length Unit="in">0.25</Length>
        </BulkHead>
        <CenteringRing>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>RA-1320</PartNumber>
            <Description>Centering ring, BT-5 to BT-20</Description>
            <Material Type="BULK">Paper, spiral kraft glassine, bulk</Material>
            <InsideDiameter Unit="in">0.543</InsideDiameter>
            <OutsideDiameter Unit="in">0.710</OutsideDiameter>
            <Length Unit="in">0.0625</Length>
        </CenteringRing>
        <EngineBlock>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>EB-20A</PartNumber>
            <Description>Engine block, BT-20</Description>
            <Material Type="BULK">Paper, spiral kraft glassine, bulk</Material>
            <InsideDiameter Unit="in">0.50</InsideDiameter>
            <OutsideDiameter Unit="in">0.710</OutsideDiameter>
            <Length Unit="in">0.25</Length>
        </EngineBlock>
        <LaunchLug>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>LL-2A</PartNumber>
            <Description>Launch lug, 1/8 in. x 2 in.</Description>
            <Material Type="BULK">Paper, spiral kraft glassine, bulk</Material>
            <InsideDiameter Unit="in">0.130</InsideDiameter>
            <OutsideDiameter Unit="in">0.156</OutsideDiameter>
            <Length Unit="in">2.0</Length>
        </LaunchLug>
        <RailButton>
            <Manufacturer>Rail-Buttons.com</Manufacturer>
            <PartNumber>1010</PartNumber>
            <Description>Standard 1010 rail button</Description>
            <Material Type="BULK">Delrin</Material>
            <OuterDiameter Unit="in">0.4375</OuterDiameter>
            <InnerDiameter Unit="in">0.2285</InnerDiameter>
            <Height Unit="in">0.2975</Height>
            <BaseHeight Unit="in">0.0730</BaseHeight>
            <FlangeHeight Unit="in">0.0730</FlangeHeight>
            <ScrewHeight Unit="in">0.0</ScrewHeight>
            <DragCoefficient>0.8</DragCoefficient>
            <Mass Unit="g">0.445</Mass>
            <ScrewMass Unit="g">1.395</ScrewMass>
            <NutMass Unit="lb">0.00226</NutMass>
        </RailButton>
        <Streamer>
            <Manufacturer>Top Flight Recovery</Manufacturer>
            <PartNumber>STR-2</PartNumber>
            <Description>Streamer, 2 in. x 40 in.</Description>
            <Material Type="SURFACE">Nylon fabric, ripstop, 1.7 oz actual</Material>
            <Length Unit="in">40.0</Length>
            <Width Unit="in">2.0</Width>
            <Thickness Unit="in">0.002</Thickness>
        </Streamer>
        <Parachute>
            <Manufacturer>Estes</Manufacturer>
            <PartNumber>PK-12</PartNumber>
            <Description>Parachute, 12 in., 6 lines</Description>
            <Material Type="SURFACE">Nylon fabric, ripstop, 1.7 oz actual</Material>
            <Diameter Unit="in">12.0</Diameter>
            <Sides>6</Sides>
            <PackedDiameter Unit="in">0.7</PackedDiameter>
            <PackedLength Unit="in">1.5</PackedLength>
            <DragCoefficient>0.75</DragCoefficient>
            <LineCount>6</LineCount>
            <LineLength Unit="in">12.0</LineLength>
            <LineMaterial Type="LINE">Nylon Paracord, 110 lb, 1/16 in. dia.</LineMaterial>
        </Parachute>
    </Components>
</OpenRocketComponent>
