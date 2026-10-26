package probe;

import java.io.ByteArrayInputStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import com.google.inject.Guice;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.material.Material;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.TypedKey;
import info.openrocket.core.preset.TypedPropertyMap;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.startup.Application;

/**
 * Milestone 3 run 1, part O1 (from the M3 scouting's OrcEdgeProbe and OrcEdgeProbe2): feeds
 * OpenRocketComponentLoader, OpenRocket's reader of .orc component preset files, edge-case documents
 * and prints what comes out of each, on OpenRocket's core with ServicesForTesting. These answers are how
 * QtRocket knows JAXB's rules for the .orc files (the binding of the DTOs in info.openrocket.core.preset
 * .xml): which elements and attributes count, which text an element has, and how a double, an int, a
 * boolean and an enum are read from it. QtRocket reimplements those rules from these answers
 * (include/QtRocket/file/preset/JaxbLexical.h, OrcElement.h). tests/core/file/preset/OrcEdgeCases.h
 * builds the same documents (the byte count and the SHA-256 of each are printed to check that) and the
 * tests beside it pin what this prints by hand (expected.txt; see ../README.md).
 *
 * Batches 1 (cases 01 to 82) and 2 (N, I, B, S and T cases) are the scouting's 178 cases, unchanged but
 * for the output: the document's size and digest, every key of a preset including those whose value is
 * null (the scouting's probe stopped at them with ComponentPreset.get()'s BugException), and an image as
 * its bytes. Batch 3 (the X cases) adds what the first two leave open: whitespace-only and other
 * booleans, more int and double texts, element content around child elements, comments and processing
 * instructions, xsi:nil, references in attribute values, and the Thumbnail's base64 text.
 */
public final class OrcEdgeProbe {

	static final String HEAD = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	static final String MATS = "<Materials><Material UnitsOfMeasure=\"kg/m3\"><Name>Cardboard</Name><Density>680.0</Density><Type>BULK</Type></Material>"
			+ "<Material UnitsOfMeasure=\"g/cm3\"><Name>Paper test</Name><Density>0.8</Density><Type>BULK</Type></Material></Materials>";
	static final String DIMS = "<InsideDiameter Unit=\"in\">0.95</InsideDiameter><OutsideDiameter Unit=\"in\">0.976</OutsideDiameter><Length Unit=\"in\">18.0</Length>";

	static String doc(String components) {
		return HEAD + "<OpenRocketComponent><Version>0.1</Version>" + MATS + "<Components>" + components + "</Components></OpenRocketComponent>";
	}

	static String tube(String inner) {
		return "<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>BT-X</PartNumber><Description>d</Description>"
				+ "<Material Type=\"BULK\">Cardboard</Material>" + inner + "</BodyTube>";
	}

	public static void main(String[] args) throws Exception {
		Locale.setDefault(Locale.US);
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule())));
		batch1();
		batch2();
		batch3();
	}

	/** The scouting's OrcEdgeProbe: structure, units, materials, required elements, encodings. */
	static void batch1() throws Exception {
		Map<String, String> cases = new LinkedHashMap<>();
		cases.put("01 baseline", doc(tube(DIMS)));
		cases.put("02 unknown element in component", doc(tube(DIMS + "<Color>red</Color>")));
		cases.put("03 unknown component kind", doc("<FinSet><Manufacturer>Estes</Manufacturer></FinSet>" + tube(DIMS)));
		cases.put("04 missing Length", doc(tube("<InsideDiameter Unit=\"in\">0.95</InsideDiameter><OutsideDiameter Unit=\"in\">0.976</OutsideDiameter>")));
		cases.put("05 missing Manufacturer", doc("<BodyTube><PartNumber>BT-X</PartNumber>" + DIMS + "</BodyTube>"));
		cases.put("06 missing PartNumber", doc("<BodyTube><Manufacturer>Estes</Manufacturer>" + DIMS + "</BodyTube>"));
		cases.put("07 bad number abc", doc(tube(DIMS.replace(">18.0<", ">abc<"))));
		cases.put("08 empty number", doc(tube(DIMS.replace(">18.0<", "><"))));
		cases.put("09 number with spaces", doc(tube(DIMS.replace(">18.0<", ">  18.0 \n<"))));
		cases.put("10 number 1.8e1", doc(tube(DIMS.replace(">18.0<", ">1.8e1<"))));
		cases.put("11 number +18", doc(tube(DIMS.replace(">18.0<", ">+18<"))));
		cases.put("12 number NaN", doc(tube(DIMS.replace(">18.0<", ">NaN<"))));
		cases.put("13 number INF", doc(tube(DIMS.replace(">18.0<", ">INF<"))));
		cases.put("14 number Infinity", doc(tube(DIMS.replace(">18.0<", ">Infinity<"))));
		cases.put("15 number 18d", doc(tube(DIMS.replace(">18.0<", ">18d<"))));
		cases.put("16 number 0x12p0", doc(tube(DIMS.replace(">18.0<", ">0x12p0<"))));
		cases.put("17 number 18,0", doc(tube(DIMS.replace(">18.0<", ">18,0<"))));
		cases.put("18 number .5", doc(tube(DIMS.replace(">18.0<", ">.5<"))));
		cases.put("19 number -0", doc(tube(DIMS.replace(">18.0<", ">-0<"))));
		cases.put("20 unknown Unit", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"furlong\">"))));
		cases.put("21 no Unit attribute", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length>"))));
		cases.put("22 Unit in/64", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"in/64\">"))));
		cases.put("23 Unit ft", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"ft\">"))));
		cases.put("24 Unit with space", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\" in\">"))));
		cases.put("25 two Length elements", doc(tube(DIMS + "<Length Unit=\"in\">20.0</Length>")));
		cases.put("26 material not in section", doc(tube(DIMS).replace(">Cardboard<", ">Unobtainium<")));
		cases.put("27 material bad Type", doc(tube(DIMS).replace("Type=\"BULK\">Cardboard", "Type=\"FOO\">Cardboard")));
		cases.put("28 material no Type", doc(tube(DIMS).replace(" Type=\"BULK\">Cardboard", ">Cardboard")));
		cases.put("29 material g/cm3 section", doc(tube(DIMS).replace(">Cardboard<", ">Paper test<")));
		cases.put("30 material name with spaces", doc(tube(DIMS).replace(">Cardboard<", "> Cardboard <")));
		cases.put("31 no Material", doc(tube(DIMS).replace("<Material Type=\"BULK\">Cardboard</Material>", "")));
		cases.put("32 Mass in g", doc(tube(DIMS + "<Mass Unit=\"g\">10</Mass>")));
		cases.put("33 Mass unknown unit", doc(tube(DIMS + "<Mass Unit=\"stone\">10</Mass>")));
		cases.put("34 wrong root", HEAD + "<Foo><Components>" + tube(DIMS) + "</Components></Foo>");
		cases.put("35 malformed", HEAD + "<OpenRocketComponent><Components>" + tube(DIMS));
		cases.put("36 empty", "");
		cases.put("37 namespace root", HEAD + "<OpenRocketComponent xmlns=\"urn:x\"><Components>" + tube(DIMS) + "</Components></OpenRocketComponent>");
		cases.put("38 no Version", HEAD + "<OpenRocketComponent>" + MATS + "<Components>" + tube(DIMS) + "</Components></OpenRocketComponent>");
		cases.put("39 Version 9.9", doc(tube(DIMS)).replace("<Version>0.1</Version>", "<Version>9.9</Version>"));
		cases.put("40 Legacy element", doc(tube(DIMS)).replace("<Version>0.1</Version>", "<Version>0.1</Version><Legacy>no</Legacy>"));
		cases.put("41 no Components", HEAD + "<OpenRocketComponent><Version>0.1</Version>" + MATS + "</OpenRocketComponent>");
		cases.put("42 components outside wrapper", HEAD + "<OpenRocketComponent>" + tube(DIMS) + "</OpenRocketComponent>");
		cases.put("43 two Components wrappers", HEAD + "<OpenRocketComponent><Components>" + tube(DIMS) + "</Components><Components>" + tube(DIMS).replace("BT-X", "BT-Y") + "</Components></OpenRocketComponent>");
		cases.put("44 material section bad uom", doc(tube(DIMS)).replace("UnitsOfMeasure=\"kg/m3\"", "UnitsOfMeasure=\"lb/in3\""));
		cases.put("45 material section bad density", doc(tube(DIMS)).replace("<Density>680.0</Density>", "<Density>x</Density>"));
		cases.put("46 material section no uom", doc(tube(DIMS)).replace(" UnitsOfMeasure=\"kg/m3\"", ""));
		cases.put("47 material section Group", doc(tube(DIMS)).replace("<Type>BULK</Type></Material>", "<Type>BULK</Type><Group>Woods</Group></Material>"));
		cases.put("48 material section bad Group", doc(tube(DIMS)).replace("<Type>BULK</Type></Material>", "<Type>BULK</Type><Group>Rocks</Group></Material>"));
		cases.put("49 material section bad Type", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type>ROCK</Type></Material><Material"));
		cases.put("50 Filled yes", doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>yes</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>"));
		cases.put("51 Filled 1", doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>1</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>"));
		cases.put("52 Filled TRUE", doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>TRUE</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>"));
		String chute = "<Parachute><Manufacturer>Estes</Manufacturer><PartNumber>PK</PartNumber><Description>chute</Description><Material Type=\"SURFACE\">Ripstop nylon</Material><Diameter Unit=\"in\">12</Diameter><Sides>6</Sides><LineCount>6</LineCount><LineLength Unit=\"in\">12</LineLength><LineMaterial Type=\"LINE\">Carpet Thread</LineMaterial></Parachute>";
		cases.put("53 parachute baseline", doc(chute));
		cases.put("54 parachute no LineCount", doc(chute.replace("<LineCount>6</LineCount>", "")));
		cases.put("55 parachute LineCount 6.0", doc(chute.replace("<LineCount>6</LineCount>", "<LineCount>6.0</LineCount>")));
		cases.put("56 parachute LineCount abc", doc(chute.replace("<LineCount>6</LineCount>", "<LineCount>abc</LineCount>")));
		cases.put("57 parachute no LineLength", doc(chute.replace("<LineLength Unit=\"in\">12</LineLength>", "")));
		cases.put("58 parachute DragCoefficient Unit in", doc(chute.replace("<Sides>", "<DragCoefficient Unit=\"in\">0.8</DragCoefficient><Sides>")));
		cases.put("59 parachute line material BULK", doc(chute.replace("Type=\"LINE\">Carpet Thread", "Type=\"BULK\">Cardboard")));
		cases.put("60 parachute canopy BULK", doc(chute.replace("Type=\"SURFACE\">Ripstop nylon", "Type=\"BULK\">Cardboard")));
		cases.put("61 nose cone bad shape", doc("<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber><Material Type=\"BULK\">Cardboard</Material><Shape>BLUNT</Shape><OutsideDiameter>0.02</OutsideDiameter><ShoulderDiameter>0.019</ShoulderDiameter><ShoulderLength>0.01</ShoulderLength><Length>0.05</Length></NoseCone>"));
		cases.put("62 nose cone shape lowercase", doc("<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber><Material Type=\"BULK\">Cardboard</Material><Shape>ogive</Shape><OutsideDiameter>0.02</OutsideDiameter><ShoulderDiameter>0.019</ShoulderDiameter><ShoulderLength>0.01</ShoulderLength><Length>0.05</Length></NoseCone>"));
		cases.put("63 nose cone no shoulder", doc("<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber><Material Type=\"BULK\">Cardboard</Material><Shape>OGIVE</Shape><OutsideDiameter>0.02</OutsideDiameter><Length>0.05</Length><Mass>0.01</Mass></NoseCone>"));
		cases.put("64 transition no fore shoulder", doc("<Transition><Manufacturer>Estes</Manufacturer><PartNumber>TR</PartNumber><Material Type=\"BULK\">Cardboard</Material><Shape>CONICAL</Shape><ForeOutsideDiameter>0.02</ForeOutsideDiameter><AftOutsideDiameter>0.03</AftOutsideDiameter><AftShoulderDiameter>0.029</AftShoulderDiameter><AftShoulderLength>0.01</AftShoulderLength><Length>0.05</Length></Transition>"));
		cases.put("65 streamer no thickness", doc("<Streamer><Manufacturer>Estes</Manufacturer><PartNumber>ST</PartNumber><Material Type=\"SURFACE\">Ripstop nylon</Material><Length>1</Length><Width>0.05</Width></Streamer>"));
		cases.put("66 rail button no NutMass", doc("<RailButton><Manufacturer>Estes</Manufacturer><PartNumber>RB</PartNumber><Material Type=\"BULK\">Cardboard</Material><InnerDiameter>0.005</InnerDiameter><OuterDiameter>0.01</OuterDiameter><Height>0.01</Height><BaseHeight>0.002</BaseHeight><FlangeHeight>0.002</FlangeHeight><ScrewHeight>0.001</ScrewHeight><ScrewMass>0.001</ScrewMass></RailButton>"));
		cases.put("67 tube no OD and no ID", doc("<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>BT</PartNumber><Length>0.1</Length></BodyTube>"));
		cases.put("68 element order reversed", doc("<BodyTube><Length Unit=\"in\">18.0</Length><OutsideDiameter Unit=\"in\">0.976</OutsideDiameter><InsideDiameter Unit=\"in\">0.95</InsideDiameter><Material Type=\"BULK\">Cardboard</Material><PartNumber>BT-X</PartNumber><Manufacturer>Estes</Manufacturer></BodyTube>"));
		cases.put("69 CDATA and comment in number", doc(tube(DIMS.replace(">18.0<", "><![CDATA[18]]><!-- c -->.5<"))));
		cases.put("70 attribute with namespace prefix", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length xmlns:q=\"urn:q\" q:Unit=\"ft\">"))));
		cases.put("71 element in other namespace", doc(tube(DIMS + "<q:Length xmlns:q=\"urn:q\">99</q:Length>")));
		cases.put("72 DOCTYPE with entity", HEAD + "<!DOCTYPE OpenRocketComponent [<!ENTITY len \"18.0\">]><OpenRocketComponent><Components>" + tube(DIMS.replace(">18.0<", ">&len;<")) + "</Components></OpenRocketComponent>");
		cases.put("73 Manufacturer with spaces", doc(tube(DIMS).replace(">Estes<", ">  Estes  <")));
		cases.put("74 same preset twice", doc(tube(DIMS) + tube(DIMS)));
		cases.put("75 thumbnail", doc(tube(DIMS + "<Thumbnail>aGVsbG8=</Thumbnail>")));
		cases.put("76 thumbnail bad base64", doc(tube(DIMS + "<Thumbnail>!!!</Thumbnail>")));
		cases.put("77 version attribute on root", doc(tube(DIMS)).replace("<OpenRocketComponent>", "<OpenRocketComponent version=\"2\">"));
		cases.put("78 utf8 BOM", "\ufeff" + doc(tube(DIMS)));
		cases.put("79 negative length", doc(tube(DIMS.replace(">18.0<", ">-18.0<"))));
		cases.put("80 huge exponent", doc(tube(DIMS.replace(">18.0<", ">1e999<"))));
		for (Map.Entry<String, String> c : cases.entrySet()) {
			run(c.getKey(), c.getValue().getBytes(StandardCharsets.UTF_8));
		}
		run("81 latin1 bytes in name (invalid UTF-8)", doc(tube(DIMS)).replace(">d<", ">caf\u00e9<").getBytes(StandardCharsets.ISO_8859_1));
		run("82 UTF-16 document", doc(tube(DIMS)).replace("UTF-8", "UTF-16").getBytes(StandardCharsets.UTF_16));
	}

	/** The scouting's OrcEdgeProbe2: the lexical rules of numbers, integers, booleans, enums and text. */
	static void batch2() throws Exception {
		Map<String, String> cases = new LinkedHashMap<>();
		String[] numbers = { "18", "18.", "1.8E1", "1.8e+1", "0x1.2p4", "0X12", "18f", "18 ", "\t18\n", "1 8", "--18", "+-18", "1e", "e1", ".", "-.5", "5.-", "\u00a018", "\u200918", "\u0661\u0668", "18\u00a0", "1_8", "0018", "-INF", " INF ", "NaN ", "+NaN", "1e-400", "4.9e-324", "2.2250738585072011e-308", "9007199254740993", "0.1", "1.7976931348623157e309" };
		int i = 0;
		for (String n : numbers) {
			cases.put(String.format("N%02d double [%s]", i++, n.replace("\n", "\\n").replace("\t", "\\t")),
					doc(tube(DIMS.replace("<Length Unit=\"in\">18.0</Length>", "<Length Unit=\"m\">" + n + "</Length>"))));
		}
		String chute = "<Parachute><Manufacturer>Estes</Manufacturer><PartNumber>PK</PartNumber><Material Type=\"SURFACE\">Ripstop nylon</Material><Diameter>0.3</Diameter><Sides>SIDES</Sides><LineCount>6</LineCount><LineLength>0.3</LineLength></Parachute>";
		String[] ints = { "6", " 6 ", "+6", "-6", "6.0", "1 2", "5-", "--5", "2147483648", "99999999999", "", "0x6", "\u0666", "6e0", "+", "-" };
		i = 0;
		for (String n : ints) {
			cases.put(String.format("I%02d int [%s]", i++, n), doc(chute.replace("SIDES", n)));
		}
		String bh = "<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>FILLED</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>";
		String[] bools = { "true", "false", "1", "0", " true ", "\ntrue\n", "True", "t", "tr", "truex", "true1", "f", "fals", "falsey", "2", "", "yes", "01", "10", " 1 " };
		i = 0;
		for (String b : bools) {
			cases.put(String.format("B%02d bool [%s]", i++, b.replace("\n", "\\n")), doc(bh.replace("FILLED", b)));
		}
		String nc = "<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber><Material Type=\"BULK\">Cardboard</Material><Shape>SHAPE</Shape><OutsideDiameter>0.02</OutsideDiameter><ShoulderDiameter>0.019</ShoulderDiameter><ShoulderLength>0.01</ShoulderLength><Length>0.05</Length></NoseCone>";
		String[] shapes = { "OGIVE", " OGIVE ", "\nOGIVE\n", "OGIVE\u00a0", "Ogive", "HAACK", "POWER" };
		i = 0;
		for (String s : shapes) {
			cases.put(String.format("S%02d shape [%s]", i++, s.replace("\n", "\\n")), doc(nc.replace("SHAPE", s)));
		}
		cases.put("T01 text split by child element", doc(tube(DIMS.replace(">18.0<", ">1<b/>8<"))));
		cases.put("T02 empty Description", doc(tube(DIMS).replace("<Description>d</Description>", "<Description/>")));
		cases.put("T03 Description with entity and CR LF", doc(tube(DIMS).replace("<Description>d</Description>", "<Description> a&amp;b\r\nc&#x9;</Description>")));
		cases.put("T04 Material type attribute with spaces", doc(tube(DIMS).replace("Type=\"BULK\"", "Type=\" BULK\"")));
		cases.put("T05 Unit attribute lb on Mass", doc(tube(DIMS + "<Mass Unit=\"lb\">0.5</Mass>")));
		cases.put("T06 Unit kg on Length", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"kg\">"))));
		cases.put("T07 two Materials wrappers", doc(tube(DIMS).replace(">Cardboard<", ">Paper test<")).replace("</Materials>", "</Materials><Materials><Material UnitsOfMeasure=\"kg/m3\"><Name>Other</Name><Density>1.0</Density><Type>BULK</Type></Material></Materials>"));
		cases.put("T08 Legacy empty", doc(tube(DIMS)).replace("<Version>0.1</Version>", "<Legacy/>"));
		cases.put("T09 text inside Components", doc("stray &gt; text" + tube(DIMS) + "-->"));
		cases.put("T10 Material element with child element", doc(tube(DIMS).replace(">Cardboard<", ">Card<x/>board<")));
		cases.put("T11 Manufacturer empty", doc(tube(DIMS).replace(">Estes<", "><")));
		cases.put("T12 PartNumber empty", doc(tube(DIMS).replace(">BT-X<", "><")));
		cases.put("T13 section density uom g/m2 for BULK", doc(tube(DIMS)).replace("UnitsOfMeasure=\"kg/m3\"", "UnitsOfMeasure=\"g/m2\""));
		cases.put("T14 section Material Type with spaces", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type> BULK </Type></Material><Material"));
		cases.put("T15 section ShearModulus", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type>BULK</Type><ShearModulus>1e9</ShearModulus></Material><Material"));
		cases.put("T16 attribute Unit on root ignored and unknown attribute", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Foo=\"bar\" Unit=\"in\">"))));
		cases.put("T17 component with xmlns default", doc(tube(DIMS).replace("<BodyTube>", "<BodyTube xmlns=\"urn:x\">")));
		cases.put("T18 Length with xmlns empty", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length xmlns=\"\" Unit=\"in\">"))));
		cases.put("T19 Length with xmlns default other", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length xmlns=\"urn:x\" Unit=\"in\">"))));
		cases.put("T20 Version element with child", doc(tube(DIMS)).replace("<Version>0.1</Version>", "<Version><x/></Version>"));
		for (Map.Entry<String, String> c : cases.entrySet()) {
			run(c.getKey(), c.getValue().getBytes(StandardCharsets.UTF_8));
		}
	}

	/** Part O1's additions: what batches 1 and 2 leave open. */
	static void batch3() throws Exception {
		Map<String, String> cases = new LinkedHashMap<>();
		String[] numbers = { "+INF", "inf", "-NaN", "INF\t", "&#13;18&#13;", " ", "+.5", "1e+", "-0x1.8p1", "1&#160;", "\n", "18<b/>x", "<b/>18" };
		int i = 0;
		for (String n : numbers) {
			cases.put(String.format("XN%02d double [%s]", i++, n.replace("\n", "\\n").replace("\t", "\\t")),
					doc(tube(DIMS.replace("<Length Unit=\"in\">18.0</Length>", "<Length Unit=\"m\">" + n + "</Length>"))));
		}
		String chute = "<Parachute><Manufacturer>Estes</Manufacturer><PartNumber>PK</PartNumber><Material Type=\"SURFACE\">Ripstop nylon</Material><Diameter>0.3</Diameter><Sides>SIDES</Sides><LineCount>6</LineCount><LineLength>0.3</LineLength></Parachute>";
		String[] ints = { "+-6", "-+6", "6\t", "\n6\n", "&#13;6", "-0", "4294967302", "-2147483649", " ", "6 -", "0006", "&#160;6", "6<b/>x", "<b/>x", "6<b/>", "x<b/>6", "6<b/><c/>x", " <b/>x" };
		i = 0;
		for (String n : ints) {
			cases.put(String.format("XI%02d int [%s]", i++, n.replace("\n", "\\n").replace("\t", "\\t")), doc(chute.replace("SIDES", n)));
		}
		String bh = "<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>FILLED</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>";
		String[] bools = { " ", "\t\t", "\n", "x", "x ", " 2 ", "&#13;1", "true\t", "false ", "0 0", "1\n", "Z", "t rue", "&#160;1", " f", "-", "falsetrue", "true<b/>x", "true<b/>", "x<b/>true", "true<b/>t", "true<b/>01", "true<b/>trux", "true<b/>fals", "true<b/>tr", "true<b/>tru", "true<b/>f", "true<b/>fa", "true<b/>fal", "true<b/>tx", "true<b/>fx", "true<b/> t", "true<b/>t ", "\u00a0", "\ud83d\ude80", "\u00e9" };
		i = 0;
		for (String b : bools) {
			cases.put(String.format("XB%02d bool [%s]", i++, b.replace("\n", "\\n").replace("\t", "\\t")), doc(bh.replace("FILLED", b)));
		}
		String nc = "<NoseCone><Manufacturer>Estes</Manufacturer><PartNumber>NC</PartNumber><Material Type=\"BULK\">Cardboard</Material><Shape>SHAPE</Shape><OutsideDiameter>0.02</OutsideDiameter><ShoulderDiameter>0.019</ShoulderDiameter><ShoulderLength>0.01</ShoulderLength><Length>0.05</Length></NoseCone>";
		String[] shapes = { "<![CDATA[OGIVE]]>", "OG<!-- c -->IVE", "X<b/>OGIVE", "OGIVE<b/>", "", "&#79;GIVE", "OGIVE<b>x</b>" };
		i = 0;
		for (String s : shapes) {
			cases.put(String.format("XS%02d shape [%s]", i++, s), doc(nc.replace("SHAPE", s)));
		}
		cases.put("XT01 text around a child element with text", doc(tube(DIMS.replace(">18.0<", ">1<b>2</b>8<"))));
		cases.put("XT02 text before an empty child only", doc(tube(DIMS.replace(">18.0<", ">18<b/><"))));
		cases.put("XT03 processing instruction in number", doc(tube(DIMS.replace(">18.0<", ">1<?pi x?>8<"))));
		cases.put("XT04 comment between digits", doc(tube(DIMS.replace(">18.0<", ">1<!-- c -->8<"))));
		cases.put("XT05 Description with child element text", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>a<b>c</b></Description>")));
		cases.put("XT06 Description in CDATA", doc(tube(DIMS).replace("<Description>d</Description>", "<Description><![CDATA[x<y]]></Description>")));
		cases.put("XT07 Description in two text runs around a comment", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>a<!-- c --> b</Description>")));
		cases.put("XT08 Description with characters beyond the BMP", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>\ud83d\ude80&#x1F680;</Description>")));
		cases.put("XT09 xsi:nil on Length", doc(tube(DIMS.replace("<Length Unit=\"in\">18.0</Length>", "<Length xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:nil=\"true\" Unit=\"in\">18.0</Length>"))));
		cases.put("XT10 xsi:nil on Description", doc(tube(DIMS).replace("<Description>d</Description>", "<Description xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:nil=\"true\">d</Description>")));
		cases.put("XT11 character reference in Unit", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"&#105;n\">"))));
		cases.put("XT12 tab in Unit", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"in\t\">"))));
		cases.put("XT13 tab reference in Unit", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length Unit=\"in&#9;\">"))));
		cases.put("XT14 two Unit attributes by prefix", doc(tube(DIMS.replace("<Length Unit=\"in\">", "<Length xmlns:q=\"urn:q\" q:Unit=\"ft\" Unit=\"cm\">"))));
		cases.put("XT15 Material Type attribute by reference", doc(tube(DIMS).replace("Type=\"BULK\"", "Type=\"&#66;ULK\"")));
		cases.put("XT16 PartNumber with trailing space and tab", doc(tube(DIMS).replace(">BT-X<", ">BT-X \t<")));
		cases.put("XT17 Legacy with a namespace", doc(tube(DIMS)).replace("<Version>0.1</Version>", "<Legacy xmlns=\"urn:x\"/>"));
		cases.put("XT18 Materials wrapper in another namespace", doc(tube(DIMS).replace(">Cardboard<", ">Paper test<")).replace("<Materials>", "<Materials xmlns=\"urn:x\">"));
		cases.put("XT19 root with an empty default namespace", doc(tube(DIMS)).replace("<OpenRocketComponent>", "<OpenRocketComponent xmlns=\"\">"));
		cases.put("XT20 root in a prefixed namespace", HEAD + "<p:OpenRocketComponent xmlns:p=\"urn:p\"><Components>" + tube(DIMS) + "</Components></p:OpenRocketComponent>");
		cases.put("XT21 empty Components wrapper after a full one", HEAD + "<OpenRocketComponent><Components>" + tube(DIMS) + "</Components><Components/></OpenRocketComponent>");
		cases.put("XT22 component element with text only", doc("<BodyTube>text</BodyTube>"));
		cases.put("XT23 document with only a root", HEAD + "<OpenRocketComponent/>");
		cases.put("XT24 comment and PI before the root", HEAD + "<!-- c --><?pi x?>\n" + doc(tube(DIMS)).substring(HEAD.length()));
		cases.put("XT25 Description with an empty child only", doc(tube(DIMS).replace("<Description>d</Description>", "<Description><b/></Description>")));
		cases.put("XT26 XML 1.1 control character before a number", doc(tube(DIMS.replace(">18.0<", ">&#11;18<"))).replace("version=\"1.0\"", "version=\"1.1\""));
		cases.put("XT27 XML 1.1 control character after a number", doc(tube(DIMS.replace(">18.0<", ">18&#11;<"))).replace("version=\"1.0\"", "version=\"1.1\""));
		cases.put("XT28 XML 1.1 control character before an int", doc("<Parachute><Manufacturer>Estes</Manufacturer><PartNumber>PK</PartNumber><Material Type=\"SURFACE\">Ripstop nylon</Material><Diameter>0.3</Diameter><Sides>&#11;6</Sides><LineCount>6</LineCount><LineLength>0.3</LineLength></Parachute>").replace("version=\"1.0\"", "version=\"1.1\""));
		cases.put("XT29 XML 1.1 control character before a boolean", doc("<BulkHead><Manufacturer>Estes</Manufacturer><PartNumber>BH</PartNumber><Filled>&#11;1</Filled><OutsideDiameter>0.02</OutsideDiameter><Length>0.01</Length></BulkHead>").replace("version=\"1.0\"", "version=\"1.1\""));
		cases.put("XT30 two Length elements, the second not a number", doc(tube(DIMS + "<Length Unit=\"in\">x</Length>")));
		cases.put("XT31 two Sides elements, the second not a number", doc(chute.replace("<Sides>SIDES</Sides>", "<Sides>6</Sides><Sides>x</Sides>")));
		cases.put("XT32 two Sides elements, the second empty", doc(chute.replace("<Sides>SIDES</Sides>", "<Sides>6</Sides><Sides></Sides>")));
		cases.put("XT33 two Density elements, the second not a number", doc(tube(DIMS)).replace("<Density>680.0</Density>", "<Density>680.0</Density><Density>x</Density>"));
		cases.put("XT34 two Filled elements, the second malformed", doc(bh.replace("<Filled>FILLED</Filled>", "<Filled>true</Filled><Filled>t</Filled>")));
		cases.put("XT35 two Description elements, the second empty", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>d</Description><Description></Description>")));
		cases.put("XT36 two Shape elements, the second unknown", doc(nc.replace("<Shape>SHAPE</Shape>", "<Shape>OGIVE</Shape><Shape>BLUNT</Shape>")));
		cases.put("XT37 two ShearModulus elements, the second not a number", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type>BULK</Type><ShearModulus>1e9</ShearModulus><ShearModulus>x</ShearModulus></Material><Material"));
		cases.put("XT38 Density with text after a child element", doc(tube(DIMS)).replace("<Density>680.0</Density>", "<Density>680.0<b/>x</Density>"));
		cases.put("XT39 two Density elements, the second empty", doc(tube(DIMS)).replace("<Density>680.0</Density>", "<Density>680.0</Density><Density></Density>"));
		cases.put("XT40 two Density elements, the first not a number", doc(tube(DIMS)).replace("<Density>680.0</Density>", "<Density>x</Density><Density>680.0</Density>"));
		cases.put("XT41 two ShearModulus elements, the second empty", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type>BULK</Type><ShearModulus>1e9</ShearModulus><ShearModulus></ShearModulus></Material><Material"));
		cases.put("XT42 ShearModulus with text after a child element", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type>BULK</Type><ShearModulus>1e9<b/>x</ShearModulus></Material><Material"));
		cases.put("XT43 ShearModulus not a number", doc(tube(DIMS)).replace("<Type>BULK</Type></Material><Material", "<Type>BULK</Type><ShearModulus>x</ShearModulus></Material><Material"));
		cases.put("XT44 two Length elements, the first not a number", doc(tube(DIMS.replace("<Length Unit=\"in\">18.0</Length>", "<Length Unit=\"in\">x</Length><Length Unit=\"in\">18.0</Length>"))));
		cases.put("XT45 Description with white space before a child element", doc(tube(DIMS).replace("<Description>d</Description>", "<Description> <b/>x</Description>")));
		cases.put("XT46 Description with white space after a child element", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>a<b/> </Description>")));
		cases.put("XT47 Description with text between two child elements", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>a<b/>c<d/></Description>")));
		cases.put("XT48 Description with a carriage return by reference", doc(tube(DIMS).replace("<Description>d</Description>", "<Description>a&#13;b&#13;&#10;c\rd</Description>")));
		String[] thumbnails = { "aGVsbG8", "aGVs bG8=", "aGVsbG8=\n", "", "QQ==", "QQ", "Q", "aGV!sbG8=", "====", "QUJD", " QUJD ", "QUJD=", "QU=JD", "4pyI",
				"QU==", "QUI=", "QUJDQQ==", "Q===", "=QUJ", "QU=D", "QQ==QUJD", "QUJD====", "AAAA", "////", "++++", "-_-_", "QU!D", "QU D", "QQ=", "QQ=A", "A===", "AA==", "QUJ\u00e9", "QUJDQUJD", "QUJD\tQUJD", "QR==", "QUK=" };
		i = 0;
		for (String t : thumbnails) {
			cases.put(String.format("XH%02d thumbnail [%s]", i++, t.replace("\n", "\\n").replace("\t", "\\t")), doc(tube(DIMS + "<Thumbnail>" + t + "</Thumbnail>")));
		}
		for (Map.Entry<String, String> c : cases.entrySet()) {
			run(c.getKey(), c.getValue().getBytes(StandardCharsets.UTF_8));
		}
	}

	static void run(String name, byte[] bytes) throws Exception {
		System.out.println("== " + name);
		System.out.println("document " + bytes.length + " bytes, sha256 "
				+ HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes)));
		try {
			List<ComponentPreset> presets = new ArrayList<>(new OpenRocketComponentLoader().load(new ByteArrayInputStream(bytes), "case.orc"));
			System.out.println("OK " + presets.size() + " presets");
			for (ComponentPreset p : presets) {
				TypedPropertyMap properties = properties(p);
				StringBuilder sb = new StringBuilder("  ");
				for (TypedKey<?> k : properties.keySet()) {
					sb.append(k.getName()).append('=').append(fmt(properties.get(k))).append("; ");
				}
				sb.append("digest=").append(p.getDigest());
				System.out.println(sb);
			}
		} catch (Throwable t) {
			System.out.println("THROWS " + t.getClass().getName() + ": " + escape(String.valueOf(t.getMessage())));
			Throwable c = t.getCause();
			while (c != null) {
				System.out.println("  cause " + c.getClass().getName() + ": " + escape(String.valueOf(c.getMessage())));
				c = c.getCause();
			}
		}
	}

	/** The preset's property map, whose keys are in the order the DTO put them. */
	static TypedPropertyMap properties(ComponentPreset p) throws ReflectiveOperationException {
		Field f = ComponentPreset.class.getDeclaredField("properties");
		f.setAccessible(true);
		return (TypedPropertyMap) f.get(p);
	}

	static String fmt(Object v) {
		if (v == null) {
			return "<null>";
		}
		if (v instanceof Material m) {
			return "[" + m.getType() + " '" + escape(m.getName()) + "' " + m.getDensity() + " shear " + m.getInPlaneShearModulus() + " " + m.getGroup().getDatabaseString() + (m.isUserDefined() ? " user" : " builtin") + "]";
		}
		if (v instanceof byte[] bb) {
			return "bytes" + bb.length + ":" + HexFormat.of().formatHex(bb);
		}
		return escape(String.valueOf(v));
	}

	/** {@code text} on one line: a backslash, a tab, a line feed and a carriage return escaped as in Java source. */
	static String escape(String text) {
		return text.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
	}
}
