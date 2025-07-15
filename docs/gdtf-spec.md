GDTF Version 1.2 Draft 1

# Introduction

In the lighting and entertainment industry, lighting fixtures
(luminaires and other controllable devices) have become more and more
complex, and the development of these devices happens faster than ever.
New devices are designed with very involved structures; they have
numerous complex color-mixing systems and mode dependencies. To give the
user access to the enormous flexibility of existing devices, a method of
providing accurate fixture type data, and controlling and
pre-visualizing devices quickly and precisely as possible, is needed.
The "General Device Type Format" (GDTF) is that method.

There are many different lighting consoles and software manufacturers on
the market, and all of them use different file formats and methods of
getting the fixture control information into their systems. The
development of new high-end fixtures at an amazing rate creates a lack
of available control data on the side of the console and
pre-visualization software manufacturers. Also, fixture manufacturers
are often approached directly by their clients with requests to support
them with accurate fixture types. As there are so many different
consoles and visualizers on the market, this process requires vast
knowledge of many different systems. Fixture manufacturers would need to
understand how every console or visualizer works, and how to provide the
required data. A format description is needed that not only provides all
of the required control information, but also structures it in a
hierarchical way matching that of the described device.

The lighting designer who would like to use these devices has to deal
with such obstacles. Designers often receive the device control data of
a specific new fixture later than expected. Also, the data may be
incomplete, because it was not created with the latest information from
the fixture manufacturer.

These issues demonstrate that our industry needs a standardized way of
defining the description of intelligent and complex devices. This
document defines such a data format. After the DIN SPEC has been
published, the format will continue to be developed further, but it is
important to make an initial version publicly available. Topics for
which no specifications can be made at this time, but which will soon
become necessary, are therefore already included in this DIN SPEC, but
with a note that no specifications can be made now.


## Scope

This document specifies the “General Device Type Format”(GDTF), a unified way of listing and describing the 
hierarchical and logical structure and controls of any type of controllable device (e.g. luminaires, fog machines, 
etc.) in the lighting and entertainment industry. It will be used as a foundation for the exchange of device data 
between lighting consoles, CAD and 3D-pre-visualization applications. The purpose of an existing GDTF-file is 
to reflect the real-world physical components of the devices and to provide control based on this information. 
It contains and is derived from the 3D geometry (real world or virtual) of the device. 
This document is only applicable for lighting systems and equipment used in the entertainment industry. 	

## Normative references

The following documents are referred to in the text in such a way that
some or all of their content constitutes requirements of this document.
For dated references, only the edition cited applies. For undated
references, the latest edition of the referenced document (including any
amendments) applies. 

- ANSI E1.54 2015, PLASA Standard for Color Communication in Entertainment
  Lighting 

- ANSI/IES TM-30, IES Method for Evaluating Light Source Color Rendition 

- RFC 4122, A Universally Unique IDentifier (UUID) URN Namespace (available at
  <https://www.ietf.org/rfc/rfc4122.txt>) 

- IEC 61966-2-1:1999, Multimedia systems and equipment - Colour measurement and
  management - Part 2-1: Colour management - Default RGB colour space - sRGB 

- ISO 22028-2:2013, Photography and graphic technology - Extended colour
  encodings for digital image storage, manipulation and interchange - Part 2:
  Reference output medium metric RGB colour image encoding (ROMM RGB)
  
- RFC 4122, A Universally Unique IDentifier (UUID) URN Namespace1

- OpenSoundControl http://opensoundcontrol.org/index.html


## Terms and definitions

For the purposes of this document, the following terms and definitions
apply. DIN and DKE maintain terminological databases for use in
standardization at the following addresses:

  - DIN-TERMinologieportal: available at
    <https://www.din.de/go/din-term>
  - DKE-IEV: available at <http://www.dke.de/DKE-IEV>

### General Device Type Format GDTF

descriptive name of the specification

### Fixture Type Attribute

singular mutually exclusive control function 
Note 1 to entry: Definitions of common attributes can be found in Annex A.

### Activation Group

attributes which need to be activated together to gain control over one logical function 
Note 1 to entry: As example Pan & Tilt are paired to gain control over Position.

### Feature

element that’s groups the Fixture Type Attributes into a structured way for easier access and search

### Feature Groups

element that’s groups the Fixture Type Features into a logical way for easier access and search

### DMXBreak

term used when a fixture needs more than one DMX start address

### glTF

file format for three-dimensional scenes and models
Note 1 to entry: glTF is the acronym for Graphics Language Transmission Format.

### glb

selfcontained binary file format of a glTF scene or model.

## File Format Definition

To describe the device type, a zip file with the extension
"*.gdtf" is used. The archive shall contain a description XML file and
resource files. Some of the resource files are located in a folder
structure. There are two folders defined: "./wheels" and "./models". The
folder "./models" has subfolders for a better structural overview:
- ./models/3ds
- ./models/gltf
- ./models/svg

The description.xml file
contains the description of the device type and all DMX modes as well as
all firmware revisions of the device.

```
./description.xml
./thumbnail.png
./thumbnail.svg
./wheels/gobo1.png
./wheels/gobo2.png
./models/3ds/base.3ds
./models/3ds/yoke.3ds
./models/gltf/base.glb
./models/gltf/yoke.glb
./models/3ds_low/base.3ds
./models/3ds_low/yoke.3ds
./models/gltf_low/base.glb
./models/gltf_low/yoke.glb
./models/gltf_high/base.glb
./models/gltf_high/yoke.glb
./models/3ds_high/base.3ds
./models/3ds_high/yoke.3ds
./models/svg/base.svg
./models/svg/yoke.svg
./models/svg_side/base.svg
./models/svg_side/yoke.svg
./models/svg_front/base.svg
./models/svg_front/yoke.svg

```

The ZIP archive name is specified as follows:
`<ManufacturerName>@<FixtureTypeName>@<OptionalComment>`

Example: `generic@led@comment`

UTF-8 has to be used to encode the XML file. Each XML file internally
consists of XML nodes. Each XML node could have XML attributes and XML
children. Each XML attribute has a value. If a XML attribute is not
specified, the default value of this XML attribute will be used. If the
XML attribute value is specified as a string, the format of the string
will depend on the XML attribute type. All XML attribute types are
specified in [Table 1](#user-content-table-1 ).


<div id="table-1">

##### Table 1 — *XML Attribute Value Types*

| Value Type                            | Format                | Description           |
|----|----|----|
|Uint<a id="attrtype-uint" />                       | Integer               | Unsigned integer|
|Int<a id="attrtype-int" />                         | Integer               | Signed integer|
|Hex<a id="attrtype-hex" />                         | Integer               | Number in hexadecimal  notation; Default value: 0|
|Float<a id="user-content-attrtype-float" />        | float                 | Floating point numeric; Separator: "."|
|String<a id="attrtype-string" />                   | Literal               | Text|
|Name<a id="attrtype-name" />                       | restricted Literal    | Unique object names; The allowed characters are listed in [AnnexC](#user-content-table-c1) Default value: object type with an index in parent.|
|Date<a id="attrtype-date" />                       | yyyy-mm-ddThh:mm:ss   |  Date and time corresponding to UTC +00:00 (Coordinated Universal Time): yyyy – year, mm – month, dd – day, hh – hours (24 format), mm – minutes, ss – seconds. Example: “2016-06-21T11:22:48” |
|Node<a id="attrtype-node" />                       | Name.Name.Name...     | Link to an element: “Name” is the value of the attribute “Name” of a defined XML node. The starting point defines each attribute separately. |
|ColorCIE<a id="attrtype-colorcie" />               | floatx, floaty,floatY       | CIE color representation xyY 1931|
|Vector3<a id="attrtype-vector3" />                 |  {float,float,float} | Vector with 3 float components|
|Matrix<a id="attrtype-matrix" />                   |  {float,float,float,float} <br/>{float,float,float,float} <br/>{float,float,float,float} <br/>{float,float,float,float} |  The transformation matrix consists 4 x 4 floats. Stored in a row-major order. For example, each row of the matrix is stored as a 4- component vector. The mathematical definition of the matrix is in a column-major order. For example, the matrix rotation is stored in the first three columns, and the translation is stored in the 4th column. The metric system consists of the Right- handed Cartesian Coordinates XYZ:<br/>X – from left (-X) to right (+X),<br/>Y – from the outside of the monitor (-Y) to the inside of the monitor (+Y),<br/>Z – from bottom (-Z) to top (+Z). 0,0,0 – center base. |
|Rotation<a id="attrtype-rotation"/>                | {float, float, float}<br/>{float, float, float} <br/>{float, float, float} | Rotation matrix, consist of 3\*3 floats. Stored as row-major matrix, i.e. each row of the matrix is stored as a 3-component vector. Mathematical definition of the matrix is column-major, i.e. the matrix rotation is stored in the three columns. Metric system, right-handed Cartesian coordinates XYZ:<br/>X – from left (-X) to right (+X),<br/>Y – from the outside of the monitor (-Y) to the inside of the monitor (+Y),<br/>Z – from the bottom (-Z) to the top (+Z). |
|Enum<a id="attrtype-enum"/>                        | Literal               | Possible values are predefined.|
|DMXAddress<a id="attrtype-dmxaddress"/>            |Int, Alternative format: Universe.Address | Absolute DMX address (size 4 bytes); Alternative format: Universe – integer universe number, starting with 1; Address: address within universe from 1 to 512. Format: integer |
|DMXValue<a id="attrtype-dmxvalue"/>                | Uint/n for ByteMirroring values <br/>Uint/ns for ByteShifting values |Special type to define DMX value where n is the byte count. The byte count can be individually specified without depending on the resolution of the DMX Channel.<br/> By default byte mirroring is used for the conversion. So 255/1 in a 16 bit channel will result in 65535.<br/>You can use the byte shifting operator to use byte shifting for the conversion. So 255/1s in a 16 bit channel will result in 65280. |
|GUID<a id="attrtype-guid"/>                        | XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX | Unique ID corresponding to RFC 4122: X–1 digit in hexadecimal notation. Example: “308EA87D-7164-42DE-8106-A6D273F57A51”. |
|Resource<a id="attrtype-resource"/>                | String|File name of the resource file without extension and without subfolder. |
|Pixel<a id="attrtype-pixel"/>                      | Pixel| 	Integer value representing one Pixel inside a MediaFile. Pixel count starts with zero in the top left corner.| 

The first XML node is always the XML description node: `<?xml version="1.0" encoding="UTF-8"?>`

The second XML node is the GDTF node. The attribute of this node is the
DataVersion: `<GDTF DataVersion="1.2">`

The example above shows the XML node for GDTF version 1.2.

<div id="table-2">

#### Table 2. *GDTF Node Attributes*

| XML Attribute Name | Value Type                             | Description                                                                                                                                                 |
|----|----|----|
| DataVersion        | [UInt.UInt](#user-content-attrtype-uint )| The DataVersion attribute defines the minimal version of compatibility. The Version format is “Major.Minor”, where major and minor is Uint with size 1 byte. |


</div>

## Fixture Type Node

The FixtureType node is the starting point of the description of the
fixture type within the XML file. The defined Fixture Type Node
attributes of the fixture type are specified in [table 3](#user-content-table-3 ).


<div id="table-3">

#### Table 3. *Fixture Type Node Attributes*

| XML Attribute Name | Value Type | Description  |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | Name of the fixture type. As it is based on Name, it is safe for parsing. |
| ShortName          | [String](#user-content-attrtype-string )     | Shortened name of the fixture type. Non detailed version or an abbreviation. Can use any characters or symbols. |
| LongName           | [String](#user-content-attrtype-string )     | Detailed, complete name of the fixture type, can include any characters or extra symbols. |
| Manufacturer       | [String](#user-content-attrtype-string )     | Manufacturer of the fixture type.|
| Description        | [String](#user-content-attrtype-string )     | Description of the fixture type. |
| FixtureTypeID      | [GUID](#user-content-attrtype-guid )         | Unique number of the fixture type. |
| Thumbnail          | [Resource](#user-content-attrtype-resource ) | Optional. File name without extension containing description of the thumbnail. Use the following as a resource file: <br />- png file to provide the rasterized picture. Maximum resolution of picture: 1024x1024 <br />- svg file to provide the vector graphic.  <br />- These resource files are located in the root directory of the zip file.  |
| ThumbnailOffsetX   | [Int](#user-content-attrtype-int )           | Horizontal offset in pixels from the top left of the viewbox to the insertion point on a label. Default value: 0 |
| ThumbnailOffsetY   | [Int](#user-content-attrtype-int )           | Vertical offset in pixels from the top left of the viewbox to the insertion point on a label. Default value: 0 |
| RefFT              | [GUID](#user-content-attrtype-guid )         | Optional. GUID of the referenced fixture type. |
| CanHaveChildren    | [Enum](#user-content-attrtype-enum )         | Describes if it is possible to mount other devices to this device. Value: “Yes”, “No”. Default value: “Yes” |

</div>


Fixture type node children are specified in [table 4](#user-content-table-4 ).

<div id="table-4">

#### Table 4. *Fixture Type Node Children*

| Child Node                                                | Mandatory | Description                                                                                 |
|----|----|----|
| [AttributeDefinitions](#user-content-attribute-definitions ) | Yes       | Defines all Fixture Type Attributes that are used in the fixture type.                      |
| [Wheels](#user-content-wheel-collect )                       | No        | Defines the physical or virtual color wheels, gobo wheels, media server content and others. |
| [PhysicalDescriptions](#user-content-physical-descriptions ) | No        | Contains additional physical descriptions.                                                  |
| [Models](#user-content-model-collect )                       | No        | Contains models of physically separated parts of the device.                                |
| [Geometries](#user-content-geometry-collect )                | Yes       | Describes physically separated parts of the device.                                         |
| [DMXModes](#user-content-dmx-mode-collect )                  | Yes       | Contains descriptions of the DMX modes.                                                     |
| [Revisions](#user-content-revision-collect )                 | No        | Describes the history of the fixture type.                                                  |
| [FTPresets](#user-content-fixture-type-preset-collect )              | No        | Is used to transfer user-defined and fixture type specific presets to other show files.     |
| [Protocols](#user-content-fixture-type-preset-collect)                        | No        | Is used to specify supported protocols.                                                     |


</div>

One or more sections could be empty or missing, but the order of
sections is mandatory as specified in [table 4](#user-content-table-4 ).

## Attribute Definitions

### General

This section defines the attribute definitions for the Fixture Type
Attributes.

Note 1: More information on the definitions of attributes can be found in Annex
A "Attribute Definitions"". 

Note 2: All currently defined Fixture Type Attributes can be found in Annex B
"Attribute Listing".

Note 3: All currently defined activation groups can be found in Annex B
"Attribute Listing". 

Note 4: All currently defined feature groups can be found in Annex B "Attribute
Listing".

The current attribute definition node does not have any XML attributes
(XML node `<AttributeDefinitions>`). Children of the attribute definition
are specified in [table 5](#user-content-table-5 ).

<div id="table-5">

#### Table 5. *Attribute Definition Children*

| XML node     | Mandatory | Description     |
|----|----|----|
| [ActivationGroups](#user-content-activation-group) | No        | Defines which attributes are to be activated together. For example, Pan and Tilt are in the same activation group.                      |
| [FeatureGroups](#user-content-feature-group)       | Yes       | Describes the logical grouping of attributes. For example, Gobo 1 and Gobo 2 are grouped in the feature Gobo of the feature group Gobo. |
| [Attributes](#user-content-attribute)              | Yes       | List of Fixture Type Attributes that are used. Predefindes fixtury type attributes can be found in Annex A.                             |

</div>


### Activation Groups
  
#### General

This section defines groups of Fixture Type Attributes that are intended
to be used together.

Example: Usually Pan and Tilt are Fixture Type Attributes that shall be
activated together to be able to store and recreate any position.

The current activation groups node does not have any XML attributes (XML
node `<ActivationGroups>`). As children it can have a list of a
[activation group](#user-content-activation-group ).

#### Activation Group

This section defines the activation group Attributes (XML node
`<ActivationGroup`>). Currently defined XML attributes of the activation
group are specified in [table 6](#user-content-table-6 ).

<div id="table-6">

#### Table 6. *Activation Group Attributes*

| XML Attribute Name | Value Type                        | Description                              |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name ) | The unique name of the activation group. |


</div>

The activation group does not have any children.

### Feature Groups
  
#### General

This section defines the logical grouping of Fixture Type Attributes
(XML node `<FeatureGroups>`).

Note 1: A feature group can contain more than one logical control unit. A
feature group Position shall contain PanTilt and XYZ as separate Feature. 

Note 2: Usually Pan and Tilt create a logical unit to enable position control,
so they must be grouped in a Feature PanTilt.

As children the feature groups has a list of a [feature group](#user-content-feature-group ).

#### Feature Group
  
##### General

This section defines the feature group (XML node `<FeatureGroup>`). The
currently defined XML attributes of the feature group are specified in
[table 7](#user-content-table-7 ).

<div id="table-7">

#### Table 7. *Feature Group Attributes*

| XML Attribute Name | Value Type                            | Description                           |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the feature group. |
| Pretty             | [String](#user-content-attrtype-string ) | The pretty name of the feature group. |


</div>

As children the feature group has a list of a
[feature](#user-content-feature ).

##### Feature

This section defines the feature (XML node `<Feature>`). The currently
defined XML attributes of the feature are specified in [table
8](#user-content-table-8 ).

<div id="table-8">

#### Table 8. *Feature Attributes*

| XML Attribute Name | Value Type                        | Description                     |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name ) | The unique name of the feature. |


</div>

The feature does not have any children.

### Attributes

#### General

This section defines the Fixture Type Attributes (XML node
`<Attributes>`). As children the attributes node has a list of a
[attributes](#user-content-attribute ).

#### Attribute

This section defines the Fixture Type Attribute (XML node `<Attribute>`).
The currently defined XML attributes of the attribute Node are specified
in [table 9](#user-content-table-9 ).

<div id="table-9">

#### Table 9. *XML Attributes of the Attribute*

| XML Attribute Name | Value Type | Description |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | The unique name of the attribute.                                                                                                                                                                                                                                                                                                                                                                                                    |
| Pretty             | [String](#user-content-attrtype-string )     | The pretty name of the attribute .                                                                                                                                                                                                                                                                                                                                                                                                   |
| ActivationGroup    | [Node](#user-content-attrtype-node )         | Optional link to the activation group. The starting point is the [activation groups](#user-content-activation-groups ) node.                                                                                                                                                                                                                                                                                                            |
| Feature            | [Node](#user-content-attrtype-node )         | Link to the corresponding feature. The starting point is the [feature groups](#user-content-feature-groups ) node.                                                                                                                                                                                                                                                                                                                      |
| MainAttribute      | [Node](#user-content-attrtype-node )         | Optional link to the main attribute. The starting point is the [attribute](#user-content-attribute ) node.                                                                                                                                                                                                                                                                                                                              |
| PhysicalUnit       | [Enum](#user-content-attrtype-enum )         | The currently defined unit values are: “None”, “Percent”, “Length” (m), “Mass” (kg), “Time” (s), “Temperature” (K), “LuminousIntensity”(cd), “Angle” (degree), “Force” (N), “Frequency” (Hz), “Current” (A), “Voltage” (V), “Power” (W), “Energy” (J), “Area” (m2), “Volume” (m3), “Speed” (m/s), “Acceleration” (m/s2), “AngularSpeed” (degree/s), “AngularAccc” (degree/s2), “WaveLength” (nm), “ColorComponent”. Default: “None”. |
| Color              | [ColorCIE](#user-content-attrtype-colorcie ) | Optional. Defines the color for the attribute.                                                                                                                                                                                                                                                                                                                                                                                       |
|                    |                                           |                                                                                                                                                                                                                                                                                                                                                                                                                                      |


</div>

As children the attribute node has a list of a [subphysical units](#user-content-subphysical-unit ).

##### Subphysical Unit

This section defines the Attribute Subphysical Unit (XML node `<SubPhysicalUnit>`).
The currently defined XML attributes of the subphysical unit are specified in [table 10](#user-content-table-10 ).

<div id="table-10">

#### Table 10. *XML Attributes of the Subphysical Unit*

| XML Attribute Name | Value Type                                | Description                                                                                             |
|----|----|----|
| Type               | [Enum](#user-content-attrtype-enum )      | The currently defined values are: "PlacementOffset", "Amplitude", "AmplitudeMin", "AmplitudeMax", "Duration", "DutyCycle",  "TimeOffset", "MinimumOpening", "Value", "RatioHorizontal", "RatioVertical".  |
| PhysicalUnit       | [Enum](#user-content-attrtype-enum )      | The currently defined unit values are: “None”, “Percent”, “Length” (m), “Mass” (kg), “Time” (s), “Temperature” (K), “LuminousIntensity”(cd), “Angle” (degree), “Force” (N), “Frequency” (Hz), “Current” (A), “Voltage” (V), “Power” (W), “Energy” (J), “Area” (m2), “Volume” (m3), “Speed” (m/s), “Acceleration” (m/s2), “AngularSpeed” (degree/s), “AngularAccc” (degree/s2), “WaveLength” (nm), “ColorComponent”. Default: “None”. |
| PhysicalFrom       | [Float](#user-content-attrtype-float )    | The default physical from of the subphysical unit; Unit: as defined in PhysicalUnit; Default value: 0   |
| PhysicalTo         | [Float](#user-content-attrtype-float )    | The default physical to of the subphysical unit; Unit: as defined in PhysicalUnit; Default value: 1     |


</div>

The subphysical unit does not have any children.

## Wheel Collect
  
### General

This section defines all physical or virtual wheels of the device. It
currently does not have any XML attributes (XML node `<Wheels>`). As
children wheel collect can have a list of a [wheels](#user-content-wheel ).

Note 1: Physical or virtual wheels represent the changes to the light beam
within the device. Typically color, gobo, prism, animation, content and others
are described by wheels.

### Wheel
  
#### General

Each wheel describes a single physical or virtual wheel of the fixture
type. If the real device has wheels you can change, then all wheel
configurations have to be described. Wheel has the following XML node:
`<Wheel>`. The currently defined XML attributes of the wheel are specified
in [table 11](#user-content-table-11 ).

<div id="table-11">

#### Table 11. *Wheel Attributes*

| XML Attribute Name | Value Type                        | Description                  |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name ) | The unique name of the wheel |


</div>

As children, Wheel has a list of [wheel
slots](#user-content-wheel-slot ).

#### Wheel Slot
  
##### General

The wheel slot represents a slot on the wheel (XML node `<Slot>`). The
currently defined XML attributes of the wheel slot are specified in
[table 12](#user-content-table-12 ).

<div id="table-12">

#### Table 12. *Wheel Slot Attributes*

| XML Attribute Name | Value Type | Description                                                                                                                                                                                                                                                                                                                       |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )        | The name of the wheel slot                                                                                                                                                                                                                                                                                                 |
| Color              | [ColorCIE](#user-content-attrtype-colorcie )   | Color of the wheel slot, Default value: {0.3127, 0.3290, 100.0 } (white) For Y give relative value compared to overall output defined in property Luminous Flux of related Beam Geometry (transmissive case).                                                                                                                     |
| Filter             | [Node](#user-content-attrtype-node )       | Optional. Link to filter in the physical description; Do not define color if filter is used; Starting point: Filter Collect                                                                                                                                                                                                       |
| MediaFileName | [Resource](#user-content-attrtype-resource ) | Optional. PNG file name without extension containing image for specific gobos etc. <br />- Maximum resolution of picture: 1024x1024<br />- Recommended resolution of gobo: 256x256<br />- Recommended resolution of animation wheel: 256x256<br />These resource files are located in a folder called `./wheels` in the zip archive. Default value: empty. |



</div>

Note 1: More information on the definitions of images used in wheel slots to
visualize gobos, animation wheels or color wheels can be found in Annex E
"Wheel Slot Image Definition".

The link between a slot and a [channel set](#user-content-channel-set ) is
done via the wheel slot index. The wheel slot index of a slot is derived
from the order of a wheel's slots. The wheel slot index is normalized to
1.

If the wheel slot has a prism, it has to have one or several children
called [prism facet](#user-content-prism-facet ). If the wheel slot has an
AnimationWheel, it has to have one child called [Animation
Wheel](#user-content-animation-wheel ).

##### Prism Facet

This section can only be defined for the prism wheel slot and has a
description for the prism facet (XML node `<Facet>`). The currently
defined XML attributes of the prism facet are specified in [table
13](#user-content-table-13 ).

<div id="table-13">

#### Table 13. *Wheel Slot Attributes*

| XML Attribute Name | Value Type                                | Description                                                           |
|----|----|----|
| Color              | [ColorCIE](#user-content-attrtype-colorcie ) | Color of prism facet, Default value: {0.3127, 0.3290, 100.0 } (white) |
| Rotation           | [Rotation](#user-content-attrtype-rotation ) | Specify the rotation, translation and scaling for the facet.          |


</div>

The prism facet cannot have any children.


##### Animation System

This section can only be defined for the animation system disk and it
describes the animation system behavior (XML node `<AnimationSystem>`).
The currently defined XML attributes of the AnimationSystem are
specified in [table 14](#user-content-table-14 ).

<div id="table-14">

#### Table 14. *AnimationSystem Attributes*

| XML Attribute Name | Value Type                                   | Description                                                                                                                                                                                                              |
|----|----|----|
| P1                 | [Array of Float](#user-content-attrtype-float ) | First Point of the Spline describing the path of animation system in the beam in relation to the middle of the Media File; Array of two floats; Separator of values is ","; First Float is X-axis and second is Y-axis.  |
| P2                 | [Array of Float](#user-content-attrtype-float ) | Second Point of the Spline describing the path of animation system in the beam in relation to the middle of the Media File; Array of two floats; Separator of values is ","; First Float is X-axis and second is Y-axis. |
| P3                 | [Array of Float](#user-content-attrtype-float ) | Third Point of the Spline describing the path of animation system in the beam in relation to the middle of the Media File; Array of two floats; Separator of values is ","; First Float is X-axis and second is Y-axis.  |
| Radius             | [Float](#user-content-attrtype-float )          | Radius of the circle that defines the section of the animation system which will be shown in the beam                                                                                                                    |


</div>

The AnimationSystem cannot have any children.

![media/animation\_wheel\_example.png](media/animation_wheel_example.png
"media/animation_wheel_example.png") Example of an animation
system


## Physical Descriptions
  
### General

This section describes the physical constitution of the device. It
currently does not have any XML Attributes (XML node
`<PhysicalDescriptions>`). Children of Physical Description are specified
in [table 15](#user-content-table-15 ).

<div id="table-15">

#### Table 15. *Physical Description Children*

| XML node                               | Mandatory | Description                                                                         |
|----|----|----|
| [Emitters](#user-content-emitter-collect )       | No        | Describes device emitters                                                           |
| [Filters](#user-content-filter-collect )         | No        | Describes device filters                                                            |
| [ColorSpace](#user-content-color-space )   | No        | Describes device default color space                                                       |
| [AdditionalColorSpaces](#user-content-color-spaces )   | No        | Describes additional device color spaces                                                        |
| [Gamuts](#user-content-gamuts )   | No        | Describes device gamuts                                                        |
| [DMXProfiles](#user-content-dmx-profile-collect) | No        | Describes nonlinear correlation between DMX input and physical output of a channel. |
| [CRIs](#user-content-color-rendering-index-collect)               | No        | Describes color rendering with IES TM-30-15 (99 color samples).                     |
| [Connectors](#user-content-connector-collect )   | No        | Obsolete now. See Geometry Collect, WiringObject. Describes physical connectors of the device.              |
| [Properties](#user-content-properties-collect )   | No        | Describes physical properties of the device.                                        |


</div>

### Emitter Collect
  
#### General

This section contains the description of the emitters. Emitter Collect
defines additive mixing of light sources, such as LEDs and tungsten
lamps with permanently fitted filters. It currently does not have any
XML Attributes (XML node `<Emitters>`). As children the emitter collect
has a list of a [emitter](#user-content-emitter ).

#### Emitter

This section defines the description of the emitter (XML node
<Emitter>). The currently defined XML attributes of the emitter are
specified in [table 16](#user-content-table-16 ).

<div id="table-16">

#### Table 16. *Emitter Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                                                                                                                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | Unique Name of the emitter                                                                                                                                                                                                    |
| Color              | [ColorCIE](#user-content-attrtype-colorcie ) | Approximate absolute color point if applicable. Omit for non-visible emitters (eg., UV). For Y give relative value compared to overall output defined in property Luminous Flux of related Beam Geometry (transmissive case). |
| DominantWaveLength | [Float](#user-content-attrtype-float )       | Required if color is omitted, otherwise it is optional. Dominant wavelength of the LED.                                                                                                                                       |
| DiodePart          | [String](#user-content-attrtype-string )     | Optional. Manufacturer’s part number of the diode.                                                                                                                                                                            |


</div>

As children, the Emitter has a list of
[measurements](#user-content-measurement ).

### Filter Collect
  
#### General

This section contains the description of the filters. The Filter Collect
defines subtractive mixing of light sources by filters, such as
subtractive mixing flags and media used in physical or virtual Color
Wheels. It currently does not have any XML Attributes (XML node
`<Filters>`). As children the filter collect has a list of a
[filters](#user-content-filter ).

#### Filter

This section defines the description of the filter (XML node `<Filter>`).
The currently defined XML attributes of the filter are specified in
[table 17](#user-content-table-17 ).

<div id="table-17">

#### Table 17. *Filter Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                                                                                                                                                                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | Unique Name of the filter.                                                                                                                                                                                                                                                    |
| Color              | [ColorCIE](#user-content-attrtype-colorcie ) | Approximate absolute color point when this filter is the only item fully inserted into the beam and the fixture is at maximum intensity. For Y give relative value compared to overall output defined in property Luminous Flux of related Beam Geometry (transmissive case). |


</div>

As children the Filter has a list of
[measurements](#user-content-measurement ).

### Measurement
  
#### General

The measurement defines the relation between the requested output by a
control channel and the physically achieved intensity. XML node for
measurement is `<Measurement>`. The currently defined XML attributes of
the measurement are specified in [table 18](#user-content-table-18 ).

<div id="table-18">

#### Table 18. *Measurement Attributes*

| XML Attribute Name | Value Type                          | Description                                                                                                                                                                                                                  |
|----|----|----|
| Physical           | [Float](#user-content-attrtype-float ) | For additive color mixing: uniquely given emitter intensity DMX percentage. Value range between \> 0 and \<= 100. For subtractive color mixing: uniquely given flag insertion DMX percentage. Value range between 0 and 100. |
| LuminousIntensity  | [Float](#user-content-attrtype-float ) | Used for additive color mixing: overall candela value for the enclosed set of measurement.                                                                                                                                   |
| Transmission       | [Float](#user-content-attrtype-float ) | Used for subtractive color mixing: total amount of lighting energy passed at this insertion percentage.                                                                                                                      |
| InterpolationTo    | [Enum](#user-content-attrtype-enum )   | Interpolation scheme from the previous value. The currently defined values are: "Linear", "Step", "Log"; Default: Linear                                                                                                     |


</div>

The order of the measurements corresponds to their ascending physical
values.

Additional definition for additive color mixing: It is assumed that the
physical value 0 exists and has zero output.

Additional definition for subtractive color mixing: The flag is removed
with physical value 0 and it does not affect the beam. Physical value
100 is maximally inserted and affects the beam.

Note 1: Some fixtures may vary in color response. These fixtures define
multiple measurement points and corresponding interpolations.

As children the Measurement Collect has an optional list of a
[measurement point](#user-content-measurement-point ).

#### Measurement Point

The measurement point defines the energy of a specific wavelength of a
spectrum. The XML node for measurement point is `<MeasurementPoint>`. The
defined XML attributes of the measurement points are specified in [table 
19](#user-content-table-19 ).

It is recommended, but not required, that measurement points are evenly
spaced. Regions with minimal light energy can be omitted, but the
decisive range of spectrum must be included. Recommended measurement
spacing is 1 nm. Measurement spacing should not exceed 4 nm.

<div id="table-19">

#### Table 19. *Measurement Point Attributes*

| XML Attribute Name | Value Type                          | Description                            |
|----|----|----|
| WaveLength         | [Float](#user-content-attrtype-float ) | Center wavelength of measurement (nm). |
| Energy             | [Float](#user-content-attrtype-float ) | Lighting energy (W/m2/nm)              |


</div>

The measurement point does not have any children.

### Color Space Collect
  
#### General

This section defines color spaces. Currently it does not
have any XML attributes (XML node `<AdditionalColorSpaces>`). As children, color space collect has a list of a [ColorSpace](#user-content-color-space ).

#### Color Space

This section defines the color space that is used for color mixing with
indirect RGB, Hue/Sat, xyY or CMY control input. (XML node
`<ColorSpace>`). The currently defined XML attributes of the color space
are specified in [table 20](#user-content-table-20 ).

<div id="table-20">

#### Table 20. *Color Space Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                                                                                      |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | Unique Name of the Color Space. Default Value: "Default". Note that the name need to be unique for the default colorspace and all color spaces in the AdditionalColorSpaces node. |                                                                                                                            |
| Mode               | [Enum](#user-content-attrtype-enum )         | Definition of the Color Space that used for the indirect color mixing. The defined values are "Custom", "sRGB", "ProPhoto" and "ANSI". Default Value: "sRGB"  |
| Red                | [ColorCIE](#user-content-attrtype-colorcie ) | Optional. CIE xyY of the Red Primary; this is used only if the ColorSpace is "Custom".                                                                        |
| Green              | [ColorCIE](#user-content-attrtype-colorcie ) | Optional. CIE xyY of the Green Primary; this is used only if the ColorSpace is "Custom".                                                                      |
| Blue               | [ColorCIE](#user-content-attrtype-colorcie ) | Optional. CIE xyY of the Blue Primary; this is used only if the ColorSpace is "Custom".                                                                       |
| WhitePoint         | [ColorCIE](#user-content-attrtype-colorcie ) | Optional. CIE xyY of the White Point; this is used only if the ColorSpace is "Custom".                                                                        |


</div>

The predefined modes for the color space XML Attributes are are
specified in [table 21](#user-content-table-21 ).

<div id="table-21">

#### Table 21. *Predefined Modes for Color Space Attribute Mode*

|             |                                     |                                          |                 |
|----|----|----|----|
| Mode        | sRGB                                | ProPhoto                                 | ANSI            |
| Description | Adobe sRGB, HDTV IEC 61966-2-1:1999 | Kodak ProPhoto ROMM RGB ISO 22028-2:2013 | ANSI E1.54-2015 |
| Red         | 0.6400, 0.3300, 0.2126              | 0.7347, 0.2653                           | 0.7347, 0.2653  |
| Green       | 0.3000, 0.6000, 0.7152              | 0.1596, 0.8404                           | 0.1596, 0.8404  |
| Blue        | 0.1500, 0.0600, 0.0722              | 0.0366, 0.0001                           | 0.0366, 0.001   |
| WhitePoint  | 0.3127, 0.3290, 1.0000              | 0.3457, 0.3585                           | 0.4254, 0.4044  |


</div>

The color space does not have any children.

### Gamut Collect
  
#### General

This section defines gamuts. Currently it does not
have any XML attributes (XML node `<Gamuts>`). As children, gamut collect has a list of a [Gamut](#user-content-gamut ).

#### Gamut

This section defines the color gamut of the fixture (XML node `<Gamut>`), which is the set of attainable colors by the fixture. The currently defined XML attributes of a gamut
are specified in [table 22](#user-content-table-22 ).

<div id="table-22">

#### Table 22. *Gamut Attributes*

| XML Attribute Name | Value Type                                             | Description                                                 |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )                   | Unique Name of the Gamut.                                   |
| Points             | [Array of ColorCIE](#user-content-attrtype-colorcie )  | Set of points defining the vertice of the gamut's polygon.  |


</div>

The gamut does not have any children.

### DMX Profile Collect
  
#### General

This section defines DMX profile descriptions. Currently it does not
have any XML attributes (XML node `<DMXProfiles>`). As children DMX
profile collect has a list of [DMX profiles](#user-content-dmx-profile ).

#### DMX Profile

This section defines the DMX profile description (XML node
`<DMXProfile>`). The currently defined XML attributes of the DMX profile
are specified in [table 23](#user-content-table-23 ).

<div id="table-23">

#### Table 23. *DMX Profile Attributes*

| XML Attribute Name  | Value Type                           | Description                    |
|----|----|----|
| Name                | [Name](#user-content-attrtype-name ) | Unique name of the DMX profile |


</div>

As children a DMX Profile has a list of [point](#user-content-point ).

##### Point

This section contains points to define the DMX profile (XML node `<Point>`). The currently defined XML attributes of a point
are specified in [table 24](#user-content-table-24 ).

<div id="table-24">

#### Table 24. *Point Attributes*

| XML Attribute Name  | Value Type                             | Description                    |
|----|----|----|
| DMXPercentage       | [Float](#user-content-attrtype-float ) | DMX percentage of the point; Unit: Percentage; Default value: 0 |
| CFC0                | [Float](#user-content-attrtype-float ) | Cubic Function Coefficient for x⁰; Default value: 0  |
| CFC1                | [Float](#user-content-attrtype-float ) | Cubic Function Coefficient for x;  Default value: 0  |
| CFC2                | [Float](#user-content-attrtype-float ) | Cubic Function Coefficient for x²; Default value: 0  |
| CFC3                | [Float](#user-content-attrtype-float ) | Cubic Function Coefficient for x³; Default value: 0  |


</div>

Find the Point with the biggest DMXPercentage below or equal x. If there is none, the output is expected to be 0.

Output(x) = CFC3 * (x - DMXPercent)³ + CFC2 * (x - DMXPercent)² + CFC1 * (x - DMXPercent) + CFC0

Here is an example where the output follows a function f for 75% of the DMX Range and another function g for the last 25%. The Point attributes are given to illustrate how they are defined.

![DMXProfile example](media/DMXProfile.png "DMXProfile example")

*Figure 1. DMXProfile example*

A Point does not have any children.

### Color Rendering Index Collect
  
#### General

This section contains TM-30-15 Fidelity Index (Rf) for 99 color samples.
Currently it does not have any XML attributes (XML node `<CRIs>`). As
children, CRIs has a list of [CRI groups](#user-content-color-rendering-index-group ).

#### Color Rendering Index Group
  
##### General

This section contains CRIs for a single color temperature (XML node
`<CRIGroup>`). The currently defined XML attributes of the CRI group are
specified in [table 25](#user-content-table-25 ).

<div id="table-25">

#### Table 25. *CRI Group Attributes*

| XML Attribute Name | Value Type                          | Description                                          |
|----|----|----|
| ColorTemperature   | [Float](#user-content-attrtype-float ) | Color temperature; Default value: 6000; Unit: Kelvin |



</div>

As children, the CRIGroup has an optional list of [Color Rendering Index](#user-content-color-rendering-index ).

##### Color Rendering Index

This section defines the CRI for one of the 99 color samples (XML node
`<CRI>`). The currently defined XML attributes of the measurement point
are specified in [table 26](#user-content-table-26 ).

<div id="table-26">

#### Table 26. *CRI Attributes*

| XML Attribute Name  | Value Type                        | Description                                                                             |
|----|----|----|
| CES                 | [Enum](#user-content-attrtype-enum ) | Color sample. The defined values are “CES01”, “CES02”, … “CES99”. Default Value “CES01" |
| ColorRenderingIndex | [UInt](#user-content-attrtype-uint ) | The color rendering index for this sample. Size: 1 byte; Default value: 100             |


</div>

The color rendering index does not have any children.

### Connector Collect
  
#### General

This section defined the physical connectors and is kept for backwards compatibility. From DIN SPEC 15800:2021 or GDTF v1.2 onwards physical connectors shall be decribed as WiringObjects in the Geometry Collect. 
It currently does not have any XML attributes (XML node `<Connectors>`). As children, the Connector Collect has a list of a [connectors](#user-content-connector ).

#### Connector

See Geometry Collect WiringObject. For easier transition find below the equivalent of the WiringObject.
This section defines the connector (XML node `<Connector>`). The currently
defined XML attributes of the connector are specified in [table
27](#user-content-table-27 ).

<div id="table-27">

#### Table 27. *Connector Attributes*

| XML Attribute Name | Value Type                          | Description                                                                                                                                            |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )   | Unique Name of the connector. Now: Geometry Type WiringObject, XML Attribute: Name                                                                                                                         |
| Type               | [Name](#user-content-attrtype-name )   | The type of the connector. Find a list of predefined types in [Annex D](#user-content-table-d1 ). Now: Geometry Type WiringObject, XML Attribute: ConnectorType.                                                        |
| DMXBreak           | [Uint](#user-content-attrtype-uint )   | Optional. Defines to which DMX Break this connector belongs to.  Obsolete now.                                                                                       |
| Gender             | [Int](#user-content-attrtype-int )     | Connectors where the addition of the Gender value equal 0, can be connected; Default value: 0; Male Connectors are -1, Female are +1, Universal are 0. Obsolete now.  |
| Length             | [Float](#user-content-attrtype-float ) | Defines the length of the connector's wire in meters. "0" means that there is no cable and the connector is build into the housing. Default value "0".  Obsolete now. |


</div>

The connector does not have any children.

### Properties Collect
  
#### General

This section defines the general properties of the device type (XML node
`<Properties>`). The Properties Collect currently does not have any XML
attributes. The currently defined children nodes of properties collect
are specified in [table 28](#user-content-table-28 ).

<div id="table-28">

#### Table 28. *Properties Collect*

| XML node                                                 | Amount | Description                                            |
|----|----|----|
| [OperatingTemperature](#user-content-operatingtemperature ) | 0 or 1 | Temperature range in which the device can be operated. |
| [Weight](#user-content-weight )                             | 0 or 1 | Weight of the device including all accessories.        |
| [PowerConsumption](#user-content-powerconsumption )         | Any    | Power information for a given connector.               |
| [LegHeight](#user-content-legheight )                       | 0 or 1 | Height of the legs.                                    |


</div>

#### OperatingTemperature

This section defines the ambient operating temperature range (XML node
`<OperatingTemperature>`). The currently defined XML attributes of the
OperatingTemperature are specified in [table 29](#user-content-table-29 ).

<div id="table-29">

#### Table 29. *Operating Temperature Attributes*

| XML Attribute Name | Value Type                          | Description                                                                 |
|----|----|----|
| Low                | [Float](#user-content-attrtype-float ) | Lowest temperature the device can be operated. Unit: °C. Default value: 0   |
| High               | [Float](#user-content-attrtype-float ) | Highest temperature the device can be operated. Unit: °C. Default value: 40 |


</div>

The OperatingTemperature currently does not have any children.

#### Weight

This section defines the overall weight of the device (XML node
`<Weight>`). The currently defined XML attributes of the weight are
specified in [table 30](#user-content-table-30 ).

<div id="table-30">

#### Table 30. *Weight Attributes*

| XML Attribute Name | Value Type                          | Description                                                                      |
|----|----|----|
| Value              | [Float](#user-content-attrtype-float ) | Weight of the device including all accessories. Unit: kilogram. Default value: 0 |


</div>

The weight currently does not have any children.

#### LegHeight

This section defines the height of the legs (XML node `<LegHeight>`). The
currently defined XML attributes of the LegHeight are specified in
[table 31](#user-content-table-31 ).

<div id="table-31">

#### Table 31. *Leg Height Attributes*

| XML Attribute Name | Value Type                          | Description                                                                                                      |
|----|----|----|
| Value              | [Float](#user-content-attrtype-float ) | Defines height of the legs - distance between the floor and the bottom base plate. Unit: meter. Default value: 0 |


</div>

The LegHeight currently does not have any children.


## Model Collect
  
### General

Each device is divided into smaller parts: body, yoke, head and so on.
These are called geometries. Each geometry has a separate model
description and a physical description. Model collect contains model
descriptions of the fixture parts. The model collect currently does not
have any XML attributes (XML node `<Models>`). As children, Model Collect
has a list of [model](#user-content-model ).

### Model

This section defines the type and dimensions of the model (XML node
`<Model>`). The currently defined XML attributes of the model are
specified in [table 32](#user-content-table-32 ).

<div id="table-32">

#### Table 32. *Model Attributes*

| XML Attribute Name | Value Type | Description |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )       | The unique name of the model |
| Length             | [Float](#user-content-attrtype-float )     | Unit: meter; Default value: 0 |
| Width              | [Float](#user-content-attrtype-float )     | Unit: meter; Default value: 0 |
| Height             | [Float](#user-content-attrtype-float )     | Unit: meter; Default value: 0 |
| PrimitiveType      | [Enum](#user-content-attrtype-enum )       | Type of 3D model; The currently defined values are: “Undefined”, “Cube”, “Cylinder”, “Sphere”, “Base”, “Yoke”, “Head”, “Scanner”, “Conventional”, “Pigtail”, "Base1_1", "Scanner1_1", "Conventional1_1"; TODO Default value: “Undefined” |
| File               | [Resource](#user-content-attrtype-resource )   | Optional. File name without extension and without subfolder containing description of the model. Use the following as a resource file:<br />- 3DS or GLB to file to provide 3D model.<br />- SVG file to provide the 2D symbol.<br />It is possible to add several files with the same name but different formats. Preferable format for the 3D model is GLTF. The resource files are located in subfolders of a folder called <code>./models</code>. The names of the subfolders correspond to the file format of the resource files (3ds, step, svg). The path for 3ds files would be <code>./models/3ds</code>. For glb files, it would be <code>./models/gltf</code>.</p> Software that is utilizing GDTF files should always be able to read both 3ds and GlTF file formats and should be able to write at least one of these formats. It is preferable that only one type of 3D model file formats is used within one GDTF file. |
| SVGOffsetX            | [Float](#user-content-attrtype-float )  | Offset in X from the 0,0 point to the desired insertion point of the top view svg. Unit based on the SVG. Default value: 0|
| SVGOffsetY            | [Float](#user-content-attrtype-float )  | Offset in Y from the 0,0 point to the desired insertion point of the top view svg. Unit based on the SVG. Default value: 0|
| SVGSideOffsetX        | [Float](#user-content-attrtype-float )  | Offset in X from the 0,0 point to the desired insertion point of the side view svg. Unit based on the SVG. Default value: 0|
| SVGSideOffsetY        | [Float](#user-content-attrtype-float )  | Offset in Y from the 0,0 point to the desired insertion point of the side view svg. Unit based on the SVG. Default value: 0|
| SVGFrontOffsetX       | [Float](#user-content-attrtype-float )  | Offset in X from the 0,0 point to the desired insertion point of the front view svg. Unit based on the SVG. Default value: 0|
| SVGFrontOffsetY       | [Float](#user-content-attrtype-float )  | Offset in Y from the 0,0 point to the desired insertion point of the front view svg. Unit based on the SVG. Default value: 0|

</div>

The model currently does not have any children.

All models of a device combined should not exceed a maximum vertices count of 1200 for the default mesh level of detail.

There are three level of details that you can define:

#### Table 33. *Mesh level of detail*

<div id="table-33">

| LOD  | Description  |  Folder 3DS / gltf |
|---|---|---|
| Low  | Optional; This is the mesh for fixtures that are far away from the camera. It should have 30% of the the vertexes from the default mesh vertex count.  | `3ds_low` / `gltf_low`  |
| Default  | This is the default mesh that is used for real time visualization in preprogramming tool. It should have the minimum vertex count possible, while still looking like the fixture in 3D.  | `3ds` / `gltf` |
| High  | Optional; This is high quality mesh targeting non-realtime applications, where the vertex count is not that important. There is no limit for the vertex count. | `3ds_high` / `gltf_high`  |

</div>

Low and High meshes definitions are optional. Place the file with the same name in the defined folder.


The device shall be drawn in a hanging position displaying the front
view. That results in the pan axis is Z aligned, and the tilt axis is X
aligned.

The metric system consists of the Right-handed Cartesian Coordinates
XYZ: X – from left (-X) to right (+X), Y – from the outside of the
monitor (-Y) to the inside of the monitor (+Y), Z – from bottom (-Z) to
top (+Z). 0,0,0 – center base plate. See Figure 2.

![media/Models\_device\_hanging\_1.1.png](media/Models_device_hanging_1.1.png
"media/Models_device_hanging_1.1.png") Figure 2. Device in a hanging
position – front view

The mesh of each fixture part shall be drawn around its own suspension
point. The zero point of a device does not necessarily have to contain
the offset related to the yoke, but it must be centered on its axis of
rotation. The offset is defined by the geometry and has to be related to
its parent geometry and not to the base. 

Note 1: In general, the offsets are mostly negative, because the device is
displayed in a hanging position.

![Device Offsets](media/Models_device_offsets_1.1.png) Figure 3. Offsets of the parts

In Figure 3 the green arrow displays the offset of the yoke related to
the base. The magenta arrow displays the offset of the head related to
the yoke. The offsets are to be defined by the position matrix of the
according geometry ([table 35](#user-content-table-35 ) – [table 55](#user-content-table-55 )). 
It is important that the axis of rotation of
each device part is exactly positioned (see Figure 4).


![media/Models\_device\_rotation.png](media/Models_device_rotation.png) Figure 4. Positions of rotation axis

The dimension XML attributes of model (see [table
32](#user-content-table-32 )) are always used, no matter the scaling and
ratio of the mesh file. The mesh is explicitly scaled to this dimension.
The length defines the dimension of the model on the X axis, the width
on the Y axis and the height on the Z axis.

SVG use viewboxes to align their content. The viewbox is always defined ny the
top left of the bounding box. With the attributes `SVG_VIEW_Offset_XY` you can
define the insertion point in relation to the view box.

SVG can have background filling. This filling should have the color #C8C8C8. By
this color, any software can replace it with another color.

| Type  | Description  | Folder 3DS / gltf |
|---|---|---|
| Top View  | View from top in -Z direction. | `svg` |
| Front View  | View from  fron in Y direction | `svg_front` |
| Side View  | View from  fron in -X direction  | `svg_side` |



![Base](media/svg/non-symetric/base.svg)


SVG images should be drawn in a 1:1 scale to the actual device. Use mm as base unit. Scaling operation from 3D meshes will not be applied to the SVG informations

### Regarding glTF Files

The used glTF files should follow these requirements:

 - Use the `glb` binary format
 - Only use the 2.0 version
 - Do not use extension
 - Do not use animations
 - Only use jpeg or png texture resource
 - all vertex attributes are `GL_FLOAT`

 ### Regarding SVG Files

- Use SVG 1.1 spec
- Don't embed bitmap images.
- Align the viewbox to the top left of the device

## Geometry Collect
  
### General

The physical description of the device parts is defined in the geometry
collect. Geometry collect can contain a separate geometry or a tree of
geometries. The geometry collect currently does not have any XML
attributes (XML node `<Geometries>`). The currently defined children nodes
of geometry collect are specified in [table 34](#user-content-table-34 ).

<div id="table-34">

#### Table 34. *Geometry Children Types*

| XML node                                                              | Amount | Description                                                                                    |
|----|----|----|
| [Geometry](#user-content-general-geometry )                           | Any    | General Geometry.                                                                              |
| [Axis](#user-content-geometry-type-axis )                             | Any    | Geometry with axis.                                                                            |
| [FilterBeam](#user-content-geometry-type-beam-filter )                | Any    | Geometry with a beam filter.                                                                   |
| [FilterColor](#user-content-geometry-type-color-filter )              | Any    | Geometry with color filter.                                                                    |
| [FilterGobo](#user-content-geometry-type-gobo-filter )                | Any    | Geometry with gobo.                                                                            |
| [FilterShaper](#user-content-geometry-type-shaper-filter )            | Any    | Geometry with shaper.                                                                          |
| [Beam](#user-content-geometry-type-beam )                             | Any    | Geometry that describes a light output to project.                                             |
| [MediaServerLayer](#user-content-geometry-type-media-server-layer )   | Any    | Geometry that describes a media representation layer of a media device.                        |
| [MediaServerCamera](#user-content-geometry-type-media-server-camera ) | Any    | Geometry that describes a camera or output layer of a media device.                            |
| [MediaServerMaster](#user-content-geometry-type-media-server-master ) | Any    | Geometry that describes a master control layer of a media device.                              |
| [Display](#user-content-geometry-type-display )                       | Any    | Geometry that describes a surface to display visual media.                                     |
| [GeometryReference](#user-content-geometry-type-reference )           | Any    | Reference to already described geometries.                                                     |
| [Laser](#user-content-geometry-type-laser )                           | Any    | Geometry with a laser light output.                                                            |
| [WiringObject](#user-content-geometry-type-wiring-object )            | Any    | Geometry that describes an internal wiring for power or data.                                  |
| [Inventory](#user-content-geometry-type-inventory )                   | Any    | Geometry that describes an additional item that can be used for a fixture (like a rain cover). |
| [Structure](#user-content-geometry-type-structure )                   | Any    | Geometry that describes the internal framing of an object (like members).                      |
| [Support](#user-content-geometry-type-support )                       | Any    | Geometry that describes a support like a base plate or a hoist.                                |
| [Magnet](#user-content-geometry-type-magnet )                         | Any    | Geometry that describes a point where other geometries should be attached.                     |


</div>

Note 1: Position the geometry in it's "Default" position. This is defined by
the Default Value from the DMX Channel that controls the position of that
geometry.

### General Geometry

It is a basic geometry type without specification (XML node `<Geometry>`).
The currently defined XML attributes of the geometry are specified in
[table 35](#user-content-table-35 ).

<div id="table-35">

#### Table 35. *Geometry Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                                                                                             |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of geometry. Recommendation for conventional is “Body”. Recommendation for a geometry that is representing the base housing of a moving head is “Base”. |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                                                                                                                                        |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                           |


</div>

The geometry has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


### Geometry Type Axis

This type of geometry defines device parts with a rotation axis (XML
node `<Axis>`). The currently defined XML attributes of the axis are
specified in [table 36](#user-content-table-36 ).

<div id="table-36">

#### Table 36. *Axis Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                                                                                                                                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry. Recommendation for an axis-geometry is “Yoke”. Recommendation for an axis-geometry representing the lamp housing of a moving head is “Head”. Note: The Head of a moving head is usually mounted to the Yoke. |
| Model           | [Name](#user-content-attrtype-name ) | Link to the corresponding model. Matrix                                                                                                                                                                                 |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                                                                                                 |


</div>

The axis has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


### Geometry Type Beam Filter

This type of geometry defines device parts with a beam filter (XML node
`<FilterBeam>`). The currently defined XML attributes of the beam filter
are specified in [table 37](#user-content-table-37 ).

<div id="table-37">

#### Table 37. *Beam Filter Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                                                                                                                                                                              |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry. Recommendation for beam filter limiting the diffusion of light is “BarnDoor”. Recommendation for beam filter adjusting the diameter of the beam is “Iris”. Note: BarnDoor and Iris are usually mounted to conventional. |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                                                                                                                                                                                                                         |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                                                                                                            |


</div>

The beam filter has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).

### Geometry Type Color Filter

This type of geometry is used to describe device parts which have a
color filter (XML node `<FilterColor>`). The currently defined XML
attributes of the color filter are specified in [table 38](#user-content-table-38 ).

<div id="table-38">

#### Table 38. *Color Filter Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                                                                                           |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of geometry. Recommendation for filter of a color or mechanical color changer is “FilterColor”. Note: FilterColor is usually mounted to conventional. |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                                                                                                                                      |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                         |


</div>

The color filter has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


### Geometry Type Gobo Filter

This type of geometry is used to describe device parts which have gobo
wheels (XML node `<FilterGobo>`). The currently defined XML attributes of
the gobo filter are specified in [table 39](#user-content-table-39 ).

<div id="table-39">

#### Table 39. *Gobo Filter Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                                                                                           |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry. Recommendation for filter of a gobo or mechanical gobo changer is “FilterGobo”. Note: FilterGobo is usually mounted to conventional. |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                                                                                                                                      |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                         |


</div>

The gobo filter has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


### Geometry Type Shaper Filter

This type of geometry is used to describe device parts which have a
shaper (XML node `<FilterShaper>`). The currently defined XML attributes
of the shaper filter are specified in [table 40](#user-content-table-40 ).

<div id="table-40">

#### Table 40. *Shaper Filter Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                                                                                                                 |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry; Recommendation for filter used to form the beam to a framed, triangular, or trapezoid shape, is “Shaper”. Note: Shaper is usually mounted to conventional. |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                                                                                                                                                            |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                                               |


</div>

The shaper filter has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


### Geometry Type Beam

This type of geometry is used to describe device parts which have a
light source (XML node `<Beam>`). The currently defined XML attributes of
the Beam are specified in [table 41](#user-content-table-41 ).

<div id="table-41">

#### Table 41. *Beam Attributes*

| XML Attribute Name  | Value Type                            | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
|----|----|----|
| Name                | [Name](#user-content-attrtype-name )     | The unique name of the geometry. Recommendation for a light source of a conventional or moving head or a projector is “Beam”. Note 1: Beam is usually mounted to Head or Body. Recommendation for a self-emitting single light source is “Pixel”. Note 2: Pixel is usually mounted to Head or Body. Recommendation for a number of Pixel that are controlled at the same time is “Array”. Note 3: Array is usually mounted to Head or Body. Recommendation for a light source of a moving mirror is “Mirror”. Note 4: Mirror is usually mounted to Yoke. |
| Model               | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| Position            | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| LampType            | [Enum](#user-content-attrtype-enum )     | Defines type of the light source; The currently defined types are: Discharge, Tungsten, Halogen, LED; Default value “Discharge”                                                                                                                                                                                                                                                                                                                                                                                                                          |
| PowerConsumption    | [Float](#user-content-attrtype-float )   | Power consumption; Default value: 1000; Unit: Watt                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| LuminousFlux        | [Float](#user-content-attrtype-float )   | Intensity of all the represented light emitters; Default value: 10000; Unit: lumen                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| ColorTemperature    | [Float](#user-content-attrtype-float )   | Color temperature; Default value: 6000; Unit: kelvin                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| BeamAngle           | [Float](#user-content-attrtype-float )   | Beam angle; Default value: 25.0; Unit: degree                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| FieldAngle          | [Float](#user-content-attrtype-float )   | Field angle; Default value: 25.0; Unit: degree                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| ThrowRatio          | [Float](#user-content-attrtype-float )   | Throw Ratio of the lens for BeamType Rectangle; Default value: 1; Unit: None                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| RectangleRatio      | [Float](#user-content-attrtype-float )   | Ratio from Width to Height of the Rectangle Type Beam; Default value: 1.7777; Unit: None                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| BeamRadius          | [Float](#user-content-attrtype-float )   | Beam radius on starting point. Default value: 0.05; Unit: meter.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| BeamType            | [Enum](#user-content-attrtype-enum )     | Beam Type; Specified values: "Wash", "Spot", "None", "Rectangle", "PC", "Fresnel", "Glow". Default value "Wash"
| ColorRenderingIndex | [Uint](#user-content-attrtype-uint )     | The CRI according to TM-30 is a quantitative measure of the ability of the light source showing the object color naturally as it does as daylight reference. Size 1 byte. Default value 100.                                                                                                                                                                                                                                                                                                                                                             |
| EmitterSpectrum     | [Node](#user-content-attrtype-node )     | Optional link to emitter in the physical description; use this to define the white light source of a subtractive color mixing system. Starting point: Emitter Collect; Default spectrum is a Black-Body with the defined ColorTemperature.                                                                                                                                                                                                                                                                                                                                                                   |

</div>

The beam has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


Use the Geometry Type "Beam" to describe the position of the fixture's
light output (usually the position of the lens) and not the position of
the light source inside the device. The origin of the Geometry Type
"Beam" is the origin of the rendered beam. The origin of the Geometry
Type "Beam" should not be covered by any faces of other geometries in
order to not block the rendered beam.

The `<BeamType>` describes how the Beam will be rendered.

"Wash", "Fresnel", "PC"- A conical beam with soft edges and softened field projection.

"Spot" - A conical beam with hard edges.

"Rectangle" - A pyramid-shaped beam with hard edges.

"None", "Glow" - No beam will be drawn, only the geometry will emit light
itself.

The beam geometry emits its light into negative Z direction (and Y-up).

### Geometry Type Media Server Layer

This type of geometry is used to describe the layer of a media device
that is used for representation of media files (XML node
`<MediaServerLayer>`). The currently defined XML attributes of the media
server layer are specified in [table 42](#user-content-table-42 ).

<div id="table-42">

#### Table 42. *Media Server Layer Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                       |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry.                                                                  |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model that will be used to display the alignment in media server space. |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                     |


</div>

The media server layer has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).

### Geometry Type Media Server Camera

This type of geometry is used to describe the camera or output of a
media device (XML node `<MediaServerCamera>`). The currently defined XML
attributes of the media server camera are specified in [table
43](#user-content-table-43 ).

<div id="table-43">

#### Table 43. *Media Server Camera Attributes*

| XML Attribute Name | Value Type                            | Description                                                                                       |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry.                                                                  |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model that will be used to display the alignment in media server space. |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                     |


</div>

The media server camera has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).

The media server camera-view points into the positive Y-direction (and
Z-up).

### Geometry Type Media Server Master

This type of geometry is used to describe the master control of one or
several media devices (XML node `<MediaServerMaster>`). The currently
defined XML attributes of the media server master are specified in
[table 44](#user-content-table-44 ).

<div id="table-44">

#### Table 44. *Media Server Master Attributes*

| XML Attribute Name | Value Type                            | Description                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the geometry.                              |
| Model              | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                              |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix |


</div>

The media server master has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).

### Geometry Type Display

This type of geometry is used to describe a self-emitting surface which
is used to display visual media (XML node `<Display>`). The currently
defined XML attributes of the display are specified in [table 45](#user-content-table-45 ).

<div id="table-45">

#### Table 45. *Display Attributes*

| XML Attribute Name | Value Type                                | Description                                                                               |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | The unique name of the geometry.                                                          |
| Model              | [Name](#user-content-attrtype-name )         | Link to the corresponding model.                                                          |
| Position           | [Matrix](#user-content-attrtype-matrix )     | Relative position of geometry; Default value: Identity Matrix                             |
| Texture            | [Resource](#user-content-attrtype-resource ) | Name of the mapped texture in Model file that will be swapped out for the media resource. |


</div>

The display has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).


### Geometry Type Laser
  
#### General

This type of geometry is used to describe the position of a laser's 
light output (XML node `<Laser>`). The currently
defined XML attributes of the laser are specified in [table 46](#user-content-table-46 ).

<div id="table-46">

#### Table 46. *Laser Attributes*

| XML Attribute Name | Value Type     |                    |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )      | The unique name of the geometry.                                                                               |
| Model              | [Name](#user-content-attrtype-name )      | Link to the corresponding model.                                                                               |
| Position           | [Matrix](#user-content-attrtype-matrix )  | Relative position of geometry; Default value: Identity Matrix                                                  |
| ColorType <a id="attrtype-colortype" />         | [Enum](#user-content-attrtype-enum )      | The currently defined unit values are: “RGB”, “SingleWaveLength”,  Default: RGB.                               |
| Color              | [Float](#user-content-attrtype-float)     | Required if [ColorType](#user-content-attrtype-colortype) is "SingleWaveLength"; Unit:nm (nanometers)                                            |
| OutputStrength     | [Float](#user-content-attrtype-float)     | Output Strength of the Laser; Unit: Watt                                                              |
| Emitter            | [Node](#user-content-attrtype-node )      | Optional link to the emitter group. The starting point is the [Emitter Collect](#user-content-emitter-collect ). |
| BeamDiameter       | [Float](#user-content-attrtype-float)     | Beam diameter where it leaves the projector; Unit: meter                                            |
| BeamDivergenceMin  | [Float](#user-content-attrtype-float)     | Minimum beam divergence; Unit: mrad  (milliradian)                                                             |
| BeamDivergenceMax  | [Float](#user-content-attrtype-float)     | Maximum beam divergence; Unit: mrad  (milliradian)                                                             |
| ScanAnglePan       | [Float](#user-content-attrtype-float)     | Possible Total Scan Angle Pan of the beam. Assumes symmetrical output; Unit: Degree                            |
| ScanAngleTilt      | [Float](#user-content-attrtype-float)     | Possible Total Scan Angle Tilt of the beam. Assumes symmetrical output; Unit: Degree                           |
| ScanSpeed          | [Float](#user-content-attrtype-float)     | Speed of the beam; Unit: kilo point per second                                                                 |


</div>

The laser has the same children types as the geometry collect (see
[table 34](#user-content-table-34 )).

In addition, it also has a list of supported protocols (XML node `<Protocol>`) as children.

#### Protocol

This XML node specifies the protocol for a Laser (XML node `<Protocol>`). The currently defined XML
attributes of the protocol are specified in [table 47](#user-content-table-47 ).

<div id="table-47">

#### Table 47 *Protocol attributes*

| XML Attribute Name | Value Type                                    | Description                                                                                              |
|----|----|----|
| Name          | [String](#user-content-attrtype-string ) | Name of the protocol                      |

The protocol doesn't have any children.

### Geometry Type Reference
  
#### General

The Geometry Type Reference is used to describe multiple instances of
the same geometry. Example: LED panel with multiple pixels. (XML node ).
The currently defined XML attributes of reference are specified in
[table 48](#user-content-table-48). 

Note 1: Geometry Reference also allows easier definition of the DMX Channels
for these geometries.

<div id="table-48">

#### Table 48. *Geometry Reference Attributes*

| XML Attribute Name | Value Type                            | Description    |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of geometry.                                                                                                                                                                                                                                                                               |
| Position           | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix                                                                                                                                                                                                                                              |
| Geometry           | [Name](#user-content-attrtype-name )     | Name of the referenced geometry. Only top level geometries are allowed to be referenced.                                                                                                                                                                                                                   |
| Model              | [Name](#user-content-attrtype-name )     | Optional. Link to the corresponding model. The model only replaces the model of the parent of the referenced geometry. The models of the children of the referenced geometry are not affected. The starting point is Models Collect. If model is not set, the model is taken from the referenced geometry. |


</div>

As children, the Geometry Type Reference has a list of breaks. The
count of the children depends on the number of different breaks in the
DMX channels of the referenced geometry. If the referenced geometry, for
example, has DMX channels with DMX break 2 and 4, the geometry reference
has to have 2 children. The first child with DMX offset for DMX break 2
and the second child for DMX break 4. If one or more of the DMX channels
of the referenced geometry have the special value “Overwrite” as a DMX
break, the DMX break for those channels and the DMX offsets need to be
defined.

#### Break

This XML node specifies the DMX offset for the DMX channel of the
referenced geometry (XML node `<Break>`). The currently defined XML
attributes of the break are specified in [table 49](#user-content-table-49 ).

<div id="table-49">

#### Table 49. *Break Attributes*

| XML Attribute Name | Value Type                                    | Description                                                                                              |
|----|----|----|
| DMXOffset          | [DMXAddress](#user-content-attrtype-dmxaddress ) | DMX offset; Default value:1 (Means no offset for the corresponding DMX Channel)                          |
| DMXBreak           | [Uint](#user-content-attrtype-uint )             | Defines the unique number of the DMX Break for which the Offset is given. Size: 1 byte; Default value 1. |


</div>


### Geometry Type Wiring Object
  
#### General

This type of geometry is used to describe an electrical device that can be wired (XML node `<WiringObject>`). The currently
defined XML attributes of a wiring object geometry are specified in [table 50](#user-content-table-50 ).

<div id="table-50">

#### Table 50. *Wiring Object Attributes*

| XML Attribute Name  | Value Type                                | Description                                                                                       |
|----|----|----|
| Name                | [Name](#user-content-attrtype-name )      | The unique name of the geometry. The name is also the name of the interface to the outside        |
| Model               | [Name](#user-content-attrtype-name )      | Link to the corresponding model.                                                                  |
| ConnectorType   <a id="attrtype-connectortype" />    | [Name](#user-content-attrtype-name )      | The type of the connector. Find a list of predefined types in [Annex D](#user-content-table-d1 ). This is not applicable for Component Types Fuses. Custom type of connector can also be defined, for example "Loose End".|
| Position            | [Matrix](#user-content-attrtype-matrix )  | Relative position of geometry; Default value: Identity Matrix                                     |
| ComponentType       | [Enum](#user-content-attrtype-enum )      | The type of the electrical component used. Defined values are "Input", "Output", "PowerSource", "Consumer", "Fuse", "NetworkProvider", "NetworkInput", "NetworkOutput", "NetworkInOut". |
| SignalType          | [String](#user-content-attrtype-string )  | The type of the signal used. Predefinded values are "Power", "DMX512", "Protocol", "AES", "AnalogVideo", "AnalogAudio". When you have a custom protocol, you can add it here.        |
| PinCount            | [Int](#user-content-attrtype-int )        | The number of available pins of the connector type to connect internal wiring to it.              |
| ElectricalPayLoad   | [Float](#user-content-attrtype-float )    | The electrical consumption in Watts. Only for [Consumers](#user-content-attrtype-connectortype ). Unit: Watt.                                         |
| VoltageRangeMax     | [Float](#user-content-attrtype-float )    | The voltage range's maximum value. Only for [Consumers](#user-content-attrtype-connectortype ). Unit:volt.                                           |
| VoltageRangeMin     | [Float](#user-content-attrtype-float )    | The voltage range's minimum value. Only for [Consumers](#user-content-attrtype-connectortype ). Unit: volt.                                            |
| FrequencyRangeMax     | [Float](#user-content-attrtype-float )    | The Frequency range's maximum value. Only for [Consumers](#user-content-attrtype-connectortype ). Unit: hertz.                                           |
| FrequencyRangeMin     | [Float](#user-content-attrtype-float )    | The Frequency range's minimum value. Only for [Consumers](#user-content-attrtype-connectortype ). Unit: hertz.                                            |
| MaxPayLoad          | [Float](#user-content-attrtype-float )    | The maximum electrical payload that this power source can handle. Only for [Power Sources](#user-content-attrtype-connectortype ). Unit: voltampere.        |
| Voltage             | [Float](#user-content-attrtype-float )    | The voltage output that this power source can handle. Only for [Power Sources](#user-content-attrtype-connectortype ). Unit: volt.                     |
| SignalLayer         | [Integer](#user-content-attrtype-integer )  | The layer of the Signal Type. In one device, all wiring geometry that use the same Signal Layers are connected. Special value 0: Connected to all geometries. |
| CosPhi              | [Float](#user-content-attrtype-float )    | The Power Factor of the device. Only for consumers.                                               |
| FuseCurrent         | [Float](#user-content-attrtype-float )    | The fuse value. Only for fuses. Unit: ampere.                                                                   |
| FuseRating          | [Enum](#user-content-attrtype-enum )      | Fuse Rating. Defined values are "B", "C", "D", "K", "Z".                                          |
| Orientation         | [Enum](#user-content-attrtype-enum )      | Where the pins are placed on the object. Defined values are "Left", "Right", "Top", "Bottom".     |
| WireGroup           | [String](#user-content-attrtype-string )  | Name of the group to which this wiring object belong.                                             |


</div>

The wiring object has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).
In addition, it also has pin patch (XML node `<PinPatch>`) children.

#### Pin Patch

This XML node (XML node `<PinPatch>`) specifies how the different sockets of its parent
wiring object are connected to the pins of other wiring objects. The currently
defined XML attributes of a pin patch are specified in [table 51](#user-content-table-51 ).

<div id="table-51">

#### Table 51. *Pin Patch Attributes*

| XML Attribute Name  | Value Type                               | Description                                                                                            |
|----|----|----|
| ToWiringObject      | [Node](#user-content-attrtype-node )     | Link to the wiring object connected through this pin patch.                                         |
| FromPin          | [Int](#user-content-attrtype-int )       | The pin number used by the parent wiring object to connect to the targeted wiring object "ToWiringObject". |
| ToPin            | [Int](#user-content-attrtype-int )       | The pin number used by the targeted wiring object "ToWiringObject" to connect to the parent wiring object. |


</div>

The pin patch doesn't have any children.

### Geometry Type Inventory

This type of geometry is used to describe a geometry used for the inventory (XML node `<Inventory>`). The currently
defined XML attributes of an inventory geometry are specified in
[table 52](#user-content-table-52 ).

<div id="table-52">

#### Table 52. *Inventory Attributes*

| XML Attribute Name  | Value Type                               | Description                                                    |
|----|----|----|
| Name                | [Name](#user-content-attrtype-name )     | The unique name of the geometry.                               |
| Model               | [Name](#user-content-attrtype-name )     | Link to the corresponding model.                               |
| Position            | [Matrix](#user-content-attrtype-matrix ) | Relative position of geometry; Default value: Identity Matrix  |
| Count               | [Int](#user-content-attrtype-int )       | The default count for new objects.                             |


</div>

The inventory geometry has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).

### Geometry Type Structure

This type of geometry is used to describe a structure (XML node `<Structure>`). The currently
defined XML attributes of a structure geometry are specified in
[table 53](#user-content-table-53 ).

<div id="table-53">

#### Table 53. *Structure Attributes*

| XML Attribute Name        | Value Type                                | Description                                                             |
|----|----|----|
| Name                      | [Name](#user-content-attrtype-name )      | The unique name of the geometry.                                        |
| Model                     | [Name](#user-content-attrtype-name )      | Link to the corresponding model.                                        |
| Position                  | [Matrix](#user-content-attrtype-matrix )  | Relative position of geometry; Default value: Identity Matrix           |
| LinkedGeometry            | [Name](#user-content-attrtype-name )      | The linked geometry.                                                    |
| StructureType             | [Enum](#user-content-attrtype-enum )      | The type of structure. Defined values are "CenterLineBased", "Detail".  |
| CrossSectionType  <a id="attrtype-crosssectiontype" />        | [Enum](#user-content-attrtype-enum )      | The type of cross section. Defined values are "TrussFramework", "Tube".       |
| CrossSectionHeight        | [Float](#user-content-attrtype-float )    | The height of the cross section. Only for [Tubes](#user-content-attrtype-crosssectiontype ). Unit: meter.                        |
| CrossSectionWallThickness | [Float](#user-content-attrtype-float )    | The thickness of the wall of the cross section.Only for [Tubes](#user-content-attrtype-crosssectiontype ). Unit: meter.          |
| TrussCrossSection         | [String](#user-content-attrtype-string )  | The name of the truss cross section. Only for [Trusses](#user-content-attrtype-crosssectiontype ).                  |




</div>

The structure geometry has the same children types as the geometry
collect (see [table 34](#user-content-table-34)).

### Geometry Type Support

This type of geometry is used to describe a support (XML node `<Support>`). The currently
defined XML attributes of a support geometry are specified in
[table 54](#user-content-table-54 ).

<div id="table-54">

#### Table 54. *Support Attributes*

| XML Attribute Name        | Value Type                                | Description                                                                                                                            |
|----|----|----|
| Name                      | [Name](#user-content-attrtype-name )      | The unique name of the geometry.                                                                                                       |
| Model                     | [Name](#user-content-attrtype-name )      | Link to the corresponding model.                                                                                                       |
| Position                  | [Matrix](#user-content-attrtype-matrix )  | Relative position of geometry; Default value: Identity Matrix                                                                          |
| SupportType    <a id="attrtype-supporttype" />           | [Enum](#user-content-attrtype-enum )      | The type of support. Defined values are "Rope", "GroundSupport".                                        |
| RopeCrossSection          | [String](#user-content-attrtype-string )  | The name of the rope cross section. Only for [Ropes](#user-content-attrtype-supporttype ).                                             |
| RopeOffset                | [Vector3](#user-content-attrtype-vector3 )| The Offset of the rope from bottom to top. Only for [Ropes](#user-content-attrtype-supporttype ). Unit: meter.                                     |
| CapacityX                 | [Float](#user-content-attrtype-float )    | The allowable force on the X-Axis applied to the object according to the Eurocode. Unit: N.                                            |
| CapacityY                 | [Float](#user-content-attrtype-float )    | The allowable force on the Y-Axis applied to the object according to the Eurocode. Unit: N.                                            |
| CapacityZ                 | [Float](#user-content-attrtype-float )    | The allowable force on the Z-Axis applied to the object according to the Eurocode. Unit: N.                                            |
| CapacityXX                | [Float](#user-content-attrtype-float )    | The allowable moment around the X-Axis applied to the object according to the Eurocode. Unit: N/m.                                     |
| CapacityYY                | [Float](#user-content-attrtype-float )    | The allowable moment around the Y-Axis applied to the object according to the Eurocode. Unit: N/m.                                     |
| CapacityZZ                | [Float](#user-content-attrtype-float )    | The allowable moment around the Z-Axis applied to the object according to the Eurocode.  Unit: N/m.                                    |
| ResistanceX               | [Float](#user-content-attrtype-float )    | The compression ratio for this support along the X-Axis. Unit N/m. Only for [Ground Supports](#user-content-attrtype-supporttype ).   |
| ResistanceY               | [Float](#user-content-attrtype-float )    | The compression ratio for this support along the Y-Axis. Unit N/m. Only for [Ground Supports](#user-content-attrtype-supporttype ).   |
| ResistanceZ               | [Float](#user-content-attrtype-float )    | The compression ratio for this support along the Z-Axis. Unit N/m. Only for [Ground Supports](#user-content-attrtype-supporttype ).   |
| ResistanceXX              | [Float](#user-content-attrtype-float )    | The compression ratio for this support around the X-Axis. Unit N/m. Only for [Ground Supports](#user-content-attrtype-supporttype ).  |
| ResistanceYY              | [Float](#user-content-attrtype-float )    | The compression ratio for this support around the Y-Axis. Unit N/m. Only for [Ground Supports](#user-content-attrtype-supporttype ).  |
| ResistanceZZ              | [Float](#user-content-attrtype-float )    | The compression ratio for this support around the Z-Axis. Unit N/m. Only for [Ground Supports](#user-content-attrtype-supporttype ).  |


</div>

The support geometry has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).

### Geometry Type Magnet

This type of geometry is used to describe a magnet, a point where other geometries should be attached (XML node `<Magnet>`). The currently
defined XML attributes of a magnet geometry are specified in
[table 55](#user-content-table-55 ).

<div id="table-55">

#### Table 55. *Magnet Attributes*

| XML Attribute Name        | Value Type                                | Description                                                                                     |
|----|----|----|
| Name                      | [Name](#user-content-attrtype-name )      | The unique name of the geometry.                                                                |
| Model                     | [Name](#user-content-attrtype-name )      | Link to the corresponding model.                                                                |
| Position                  | [Matrix](#user-content-attrtype-matrix )  | Relative position of geometry; Default value: Identity Matrix                                   |


</div>

The magnet geometry has the same children types as the geometry
collect (see [table 34](#user-content-table-34 )).

## DMX Mode Collect
  
### General

This section is describes all DMX modes of the device. If firmware
revisions change a DMX footprint, then such revisions should be
specified as new DMX mode. The DMX mode collect currently does not have
any attributes (XML node `<DMXModes>`). As a child the fixture type DMX
mode collect has DMX modes.

### DMX Mode

Each DMX mode describes logical control part of the device in a specific
mode (XML node `<DMXMode>`). The currently defined XML attributes of the
DMX mode are specified in [table 56](#user-content-table-56 ).

<div id="table-56">

#### Table 56. *DMX Mode Attributes*

| XML Attribute Name | Value Type                        | Description                                                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name ) | The unique name of the DMX mode                                                               |
| Description        | [String](#user-content-attrtype-string ) | Description of the DMX mode                                                               |
| Geometry           | [Name](#user-content-attrtype-name ) | Name of the first geometry in the device; Only top level geometries are allowed to be linked. |


</div>

DMX mode children are specified in [table 57](#user-content-table-57 ).

<div id="table-57">

#### Table 57. *DMX Mode Children*

| XML node                                       | Mandatory | Description                                      |
|----|----|----|
| [DMXChannels](#user-content-dmx-channel-collect ) | Yes       | Description of all DMX channels used in the mode |
| [Relations](#user-content-relation-collect )      | No        | Description of relations between channels        |
| [FTMacros](#user-content-macro-collect )      | No        | Is used to describe macros of the manufacturer.  |


</div>

#### DMX Channel Collect
  
##### General

This section defines the DMX footprint of the device. The DMX channel
collect currently does not have any attributes (XML node `<DMXChannels>`).
As children the DMX channel collect has a list of a [DMX
channels](#user-content-dmx-channel ).

##### DMX Channel
  
###### General

This section defines the DMX channel (XML node `<DMXChannel>`). The name
of a DMX channel cannot be user-defined and must consist of a geometry
name and the attribute name of the first logical channel with separator
"\_". In one DMX Mode, this combination needs to be unique. Currently
defined XML attributes of the DMX channel are specified in [table
58](#user-content-table-58).

<div id="table-58">

#### Table 58. *DMX Channel Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                                                                                                                                    |
|----|----|----|
| DMXBreak           | [Int](#user-content-attrtype-int )           | Number of the DMXBreak; Default value: 1; Special value: “Overwrite” – means that this number will be overwritten by Geometry Reference; Size: 4 bytes                                                         |
| Offset             | [Array of Int](#user-content-attrtype-int )  | Relative addresses of the current DMX channel from highest to least significant; Separator of values is ","; Special value: “None” – does not have any addresses; Default value: “None”; Size per int: 4 bytes |
| InitialFunction    | [Node](#user-content-attrtype-node )         | Link to the channel function that will be activated by default for this DMXChannel. Default value is the first channel function of the first logical function of this DMX channel.                                                                                                                            |
| Highlight          | [DMXValue](#user-content-attrtype-dmxvalue ) | Highlight value for current channel; Special value: “None”. Default value: “None”.                                                                                                                             |
| Geometry           | [Name](#user-content-attrtype-name )         | Name of the geometry the current channel controls.                                                                                                                                                             |


</div>

The `Geometry` should be the place in the tree of geometries where the function
of the DMX Channel (as defined by ChannelFunction) is located either physically
or logically. If the DMX channel doesn't have a location, put it in the top
level geometry of the geometry tree. Attributes follow a trickle down
principle, so they are inherited from top down. 

As children the DMX channel has a list of [logical
channels](#user-content-logical-channel ).

###### Logical Channel

The Fixture Type Attribute is assigned to a LogicalChannel and defines
the function of the LogicalChannel. All logical channels that are
children of the same DMX channel are mutually exclusive. In a DMX mode,
only one logical channel with the same attribute can reference the same
geometry at a time. The name of a Logical Channel cannot be user-defined
and is equal to the linked attribute name. The XML node of the logical
channel is `<LogicalChannel>`. The currently defined XML attributes of the
logical channel are specified in [table 59](#user-content-table-59 ).

<div id="table-59">

#### Table 59. *Logical Channel Attributes*

| XML Attribute Name | Value Type                          | Description                                                                                                                                                                                  |
|----|----|----|
| Attribute          | [Node](#user-content-attrtype-node )   | Link to the attribute; The starting point is the Attribute Collect (see [Annex A](#user-content-annex-a-normative-attribute-definitions)).                                                              |
| Snap               | [Enum](#user-content-attrtype-enum )   | If snap is enabled, the logical channel will not fade between values. Instead, it will jump directly to the new value.; Value: “Yes”, “No”, “On”, “Off”. Default value: “No”                 |
| Master             | [Enum](#user-content-attrtype-enum )   | Defines if all the subordinate channel functions react to a Group Control defined by the control system. Values: “None”, “Grand”, “Group”; Default value: “None”.                            |
| MibFade            | [Float](#user-content-attrtype-float ) | Minimum fade time for moves in black action. MibFade is defined for the complete DMX range. Default value: 0; Unit: second                                                                   |
| DMXChangeTimeLimit | [Float](#user-content-attrtype-float ) | Minimum fade time for the subordinate channel functions to change DMX values by the control system. DMXChangeTimeLimit is defined for the complete DMX range. Default value: 0; Unit: second |


</div>

As a child the logical channel has a list of a [channel
function](#user-content-channel-function ).

###### Channel Function

The Fixture Type Attribute is assigned to a Channel Function and defines
the function of its DMX Range. (XML node `<ChannelFunction>`). The
currently defined XML attributes of channel function are specified in
[table 60](#user-content-table-60 ).

<div id="table-60">

#### Table 60. *Channel Function Attributes*

| XML Attribute Name | Value Type                                   | Description                                                                                                                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | Unique name; Default value: Name of attribute and number of channel function.                                                                                 |
| Attribute          | [Node](#user-content-attrtype-node )         | Link to attribute; Starting point is the attributes node. Default value: “NoFeature”.                                                                         |
| OriginalAttribute  | [String](#user-content-attrtype-string )     | The manufacturer's original name of the attribute; Default: empty                                                                                             |
| DMXFrom            | [DMXValue](#user-content-attrtype-dmxvalue ) | Start DMX value; The end DMX value is calculated as a DMXFrom of the next channel function – 1 or the maximum value of the DMX channel. Default value: "0/1". |
| Default            | [DMXValue](#user-content-attrtype-dmxvalue ) | Default DMX value of channel function when activated by the control system.                                                                                   |
| PhysicalFrom       | [Float](#user-content-attrtype-float )       | Physical start value; Default value: 0                                                                                                                        |
| PhysicalTo         | [Float](#user-content-attrtype-float )       | Physical end value; Default value: 1                                                                                                                          |
| RealFade           | [Float](#user-content-attrtype-float )       | Time in seconds to move from min to max of the Channel Function; Default value: 0                                                                             |
| RealAcceleration   | [Float](#user-content-attrtype-float )       | Time in seconds to accelerate from stop to maximum velocity; Default value: 0                                                                                 |
| Wheel              | [Node](#user-content-attrtype-node )         | Optional. Link to a wheel; Starting point: Wheel Collect                                                                                                      |
| Emitter            | [Node](#user-content-attrtype-node )         | Optional. Link to an emitter in the physical description; Starting point: Emitter Collect                                                                     |
| Filter             | [Node](#user-content-attrtype-node )         | Optional. Link to a filter in the physical description; Starting point: Filter Collect                                                                        |
| ColorSpace         | [Node](#user-content-attrtype-node )         | Optional. Link to a color space in the physical description; Starting point: Physical Descriptions Collect                                                    |
| Gamut              | [Node](#user-content-attrtype-node )         | Optional. Link to a gamut in the physical description; Starting point: Gamut Collect                                                                          |
| ModeMaster         | [Node](#user-content-attrtype-node )         | Optional. Link to DMX Channel or Channel Function; Starting point DMX mode.                                                                                   |
| ModeFrom           | [DMXValue](#user-content-attrtype-dmxvalue ) | Only used together with ModeMaster; DMX start value; Default value: 0/1                                                                                       |
| ModeTo             | [DMXValue](#user-content-attrtype-dmxvalue ) | Only used together with ModeMaster; DMX end value; Default value: 0/1                                                                                         |
| DMXProfile         | [Node](#user-content-attrtype-node )         | Optional link to DMX Profile; Starting point: DMX Profile Collect                                                                                             |
| Min                | [Float](#user-content-attrtype-float )       | Minimum Physical Value that will be used for the DMX Profile. Default: Value from PhysicalFrom                                                                |
| Max                | [Float](#user-content-attrtype-float )       | Maximum Physical Value that will be used for the DMX Profile. Default: Value from PhysicalTo                                                                  |
| CustomName         | [String](#user-content-attrtype-string )     | Custom Name that can he used do adress this channel function with other command based protocols like OSC. Default: Node Name of the Channel function Example: Head_Dimmer.Dimmer.Dimmer   |



</div>

Note:  
For command based control systems, you can control the fixture by sending it a string in the following style:  
`"/FIXTURE_ID/CUSTOM_NAME_CHANNELFUCTION ,f FLOAT_VALUE_PHYSICAL"`  
or   
`"/FIXTURE_ID/CUSTOM_NAME_CHANNELFUCTION/percent ,f FLOAT_VALUE_PERCENT"`

Where:  
- FIXTURE_ID is the fixture ID is the value defined for the fixture instance.
- CUSTOM_NAME_CHANNELFUCTION is the Custom Name for the ChannelFunction. Note that all "." Separators can be replaced with "/".
- FLOAT_VALUE_PHYSICAL is the physical value that the fixture should adopt. The values will be capped by the fixture by PhysicalFrom and PhysicalTo.
- FLOAT_VALUE_PERCENT is the percent value that the fixture should adopt. The values can be between 0 and 100.


As children the channel function has list of a [channel
sets](#user-content-channel-set ) and a [sub channel
sets](#user-content-sub-channel-set ).

###### Channel Set

This section defines the channel sets of the channel function (XML node
<ChannelSet>). The currently defined XML attributes of the channel set
are specified in [table 61](#user-content-table-61 ).

<div id="table-61">

#### Table 61. *Channel Set Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                                                                                                                                                                                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | The name of the channel set. Default: Empty                                                                                                                                             |
| DMXFrom            | [DMXValue](#user-content-attrtype-dmxvalue ) | Start DMX value; The end DMX value is calculated as a DMXFrom of the next channel set – 1 or the maximum value of the current channel function; Default value: 0/1                      |
| PhysicalFrom       | [Float](#user-content-attrtype-float )       | Physical start value. Default value is the PhysicalFrom from the parent channel function.                                                                                               |
| PhysicalTo         | [Float](#user-content-attrtype-float )       | Physical end value. Default value is the PhysicalTo from the parent channel function.                                                                                                   |
| WheelSlotIndex     | [Int](#user-content-attrtype-int )           | If the channel function has a link to a wheel, a corresponding slot index shall be specified. The wheel slot index results from the order of slots of the wheel which is linked in the channel function. The wheel slot index is normalized to 1. Size: 4 bytes |


</div>

The channel set does not have any children.

###### Sub Channel Set

This section defines the sub channel sets of the channel function (XML node
<SubChannelSet>). The currently defined XML attributes of the sub channel set
are specified in [table 62](#user-content-table-62 ).

<div id="table-62">

#### Table 62. *Sub Channel Set Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                                                                                                                                                                                     |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )         | The name of the sub channel set. Default: Empty                                                                                                                                                                                                              |
| PhysicalFrom       | [Float](#user-content-attrtype-float )       | Physical start value                                                                                                                                                                                                                                         |
| PhysicalTo         | [Float](#user-content-attrtype-float )       | Physical end value                                                                                                                                                                                                                                           |
| SubPhysicalUnit    | [Node](#user-content-attrtype-node )         | Link to the sub physical unit; Starting Point: Attribute                                                                                                                                                                                                     |
| DMXProfile         | [Node](#user-content-attrtype-node )         | Optional link to the DMX Profile; Starting Point: DMX Profile Collect                                                                                                                                                                                        |


</div>

The sub channel set does not have any children.

#### Relation Collect
  
##### General

This section describes the dependencies between DMX channels and channel
functions, such as multiply and override. The relation collect currently
does not have any XML attributes (XML node `<Relations>`). As children the
relation collect has a list of a [relation](#user-content-relation ).

##### Relation

This section defines the relation between the master DMX channel and the
following logical channel (XML node `<Relation>`). The currently defined
XML attributes of the relations are specified in [table 63](#user-content-table-63 ).

<div id="table-63">

#### Table 63. *Relation Attributes*

| XML Attribute Name | Value Type                               | Description                                                      |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name )     | The unique name of the relation                                  |
| Master             | [Node](#user-content-attrtype-node )     | Link to the master DMX channel; Starting point: DMX mode         |
| Follower           | [Node](#user-content-attrtype-node )     | Link to the following channel function; Starting point: DMX mode |
| Type               | [Enum](#user-content-attrtype-enum )     | Type of the relation; Values: “Multiply”, “Override”             |


</div>

The relation does not have any children. [Listing
1](#user-content-listing-1 ) shows an example of a simple DMX mode
described in XML.

<div id="listing-1">

#### Listing 1. *DMX mode with relations*

```
<DMXMode Name="Default" Geometry="Body">

   <DMXChannels>  
       <DMXChannel Highlight="255/1" Geometry="Pixel">  
           <LogicalChannel Attribute="Dimmer" Master="Grand">  
               <ChannelFunction Attribute="Dimmer" DMXFrom="0/1">  
                   <ChannelSet Name="closed" DMXFrom="0/1" PhysicalTo="0" />  
                   <ChannelSet DMXFrom="1/1" />  
                   <ChannelSet Name="open" DMXFrom="255/1" PhysicalFrom="1" />  
               </ChannelFunction>  
           </LogicalChannel>  
       </DMXChannel>  
       <DMXChannel DMXBreak="Overwrite" Offset="1" Default="255/1" Highlight="255/1" Geometry="Pixel">  
           <LogicalChannel Attribute="ColorAdd_R">  
               <ChannelFunction Attribute="ColorAdd_R" DMXFrom="0/1"/>  
           </LogicalChannel>  
       </DMXChannel>  
       <DMXChannel DMXBreak="Overwrite" Offset="2" Default="255/1" Highlight="255/1" Geometry="Pixel">  
           <LogicalChannel Attribute="ColorAdd_G">  
               <ChannelFunction Attribute="ColorAdd_G" DMXFrom="0/1"/>  
           </LogicalChannel>  
       </DMXChannel>  
       <DMXChannel DMXBreak="Overwrite" Offset="3" Default="255/1" Highlight="255/1" Geometry="Pixel">  
           <LogicalChannel Attribute="ColorAdd_B">  
               <ChannelFunction Attribute="ColorAdd_B" DMXFrom="0/1"/>  
           </LogicalChannel>  
       </DMXChannel>  
   </DMXChannels>  
   <Relations>  
       <Relation Name="VirtualDimmer" Master="Pixel_Dimmer" Follower="Pixel_ColorAdd_R.ColorAdd_R.ColorAdd_R 1" Type="Multiply" />  
       <Relation Name="VirtualDimmer" Master="Pixel_Dimmer" Follower="Pixel_ColorAdd_G.ColorAdd_G.ColorAdd_G 1" Type="Multiply" />  
       <Relation Name="VirtualDimmer" Master="Pixel_Dimmer" Follower="Pixel_ColorAdd_B.ColorAdd_B.ColorAdd_B 1" Type="Multiply" />  
   </Relations>
</DMXMode> 

```





#### Macro Collect
 
##### General

This section describes DMX sequences to be executed by the control
system. The macro collect currently does not have any XML attributes
(XML node `<FTMacros>`). As children the macro collect has a list of a
[macro](#user-content-macro ).

##### Macro
  
###### General

This section defines a DMX sequence. (XML node `<FTMacro>`). The currently
defined XML attributes of the macro are specified in [table
64](#user-content-table-64 ).

<div id="table-64">

#### Table 64. *Macro Attributes*

| XML Attribute Name | Value Type                        | Description                   |
|----|----|----|
| Name               | [Name](#user-content-attrtype-name ) | The unique name of the macro. |
| ChannelFunction    | [Node](#user-content-attrtype-node ) | Optional. Link to channel function; Starting point [DMX Mode](#user-content-dmx-mode) |


</div>

Macro children are specified in [table 65](#user-content-table-65 )

<div id="table-65">

#### Table 65. *Macro Children*

| XML node                          | Mandatory | Description                          |
|----|----|----|
| [MacroDMX](#user-content-macro-dmx ) | No        | This section defines a DMX sequence. |


</div>

###### Macro DMX

This section defines the sequence of DMX values which are sent by a
control system. (XML node `<MacroDMX>`). As children the macro DMX has a
list of [MacroDMXStep](#user-content-macro-dmx-step ).

###### Macro DMX Step

This section defines a DMX step (XML node `<MacroDMXStep>`). The currently
defined XML attributes of the macro DMX step are specified in [table
66](#user-content-table-66 ).

<div id="table-66">

#### Table 66. *Macro DMX Step Attributes*

| XML Attribute Name | Value Type                          | Description                                          |
|----|----|----|
| Duration           | [Float](#user-content-attrtype-float ) | Duration of a step; Default value: 1; Unit: seconds. |


</div>

As children the macro DMX -Step has a list of a [DMX
Value](#user-content-dmx-value ).

###### DMX Value

This section defines the value for DMX channel (XML node
<MacroDMXValue>). The currently defined XML attributes of the DMX Value
are specified in [table 67](#user-content-table-67 ).

<div id="table-67">

#### Table 67. *DMX Value Attributes*

| XML Attribute Name | Value Type                                | Description                                                                                  |
|----|----|----|
| Value              | [DMXValue](#user-content-attrtype-dmxvalue ) | Value of the DMX channel                                                                     |
| DMXChannel         | [Node](#user-content-attrtype-node )         | Link to a DMX channel. Starting node [DMX Channel collect](#user-content-dmx-channel-collect ). |


</div>

The DMX value does not have any children.

## Revision Collect
  
### General

This section defines the history of device type. Revision collect
currently does not have any XML attributes (XML node `<Revisions>`). As
children the revision collect has a list of a
[revision](#user-content-revision ).

### Revision

This section defines one revision of a the device type (XML node
`Revision>`). Revisions are optional. Every time a GDTF file is uploaded
to the database, a revision with the actual time and UserID is created
by the database. The currently defined XML attributes of the revision
are specified in [table 68](#user-content-table-68 ).

<div id="table-68">

#### Table 68. *Revision Attributes*

| XML Attribute Name | Value Type                            | Description                                                                          |
|----|----|----|
| Text               | [String](#user-content-attrtype-string ) | User-defined text for this revision; Default value: empty                            |
| Date               | [Date](#user-content-attrtype-date )     | Revision date and time                                                               |
| UserID             | [Uint](#user-content-attrtype-uint )     | UserID of the user that has uploaded the GDTF file to the database; Default value: 0 |
| ModifiedBy         | [String](#user-content-attrtype-string ) | Name of the software that modified this revision; Default value: empty               |


</div>

The revision does not have any children.


## Fixture Type Preset Collect
  
### General

This section defines fixture type specific presets. It currently does
not have any XML attributes (XML node `FTPresets>`). As children the
Fixture Preset Collect has a list of [FTPreset](#user-content-fixture-type-preset-collect )
(fixture type preset).

### Fixture Type Preset

This section has not yet been defined (XML node `<FTPreset>`).

## Supported Protocol Collect
  
### General

If the device supports one or several additional protocols, these
protocol specific information have to be specified. The supported
protocol collect currently does not have any XML attributes (XML node
`<Protocols>`). Children of supported protocol collect are specified in
[table 69](#user-content-table-69 ).

<div id="table-69">

#### Table 69. *Supported Protocol Collect Children*

| XML node                                                   | Mandatory | Description                            |
|----|----|----|
| [RDM](#user-content-rdm-section )                             | No        | Describes RDM information              |
| [Art-Net](#user-content-art-net-section )                     | No        | Describes Art-Net information          |
| [sACN](#user-content-streaming-acn-section )                  | No        | Describes sACN information             |
| [PosiStageNet](#user-content-posi-stage-net-section )         | No        | Describes PosiStageNet information     |
| [OpenSoundControl](#user-content-open-sound-control-section ) | No        | Describes OpenSoundControl information |
| [CITP](#user-content-citp-section )                           | No        | Describes CITP information             |


</div>

### RDM Section

If the device supports the RDM protocol, this section defines the
corresponding information (XML node `<FTRDM>`). The currently defined XML
attributes of RDM are specified in [table 70](#user-content-table-70 ).

<div id="table-70">

#### Table 70. *RDM Attributes*

| XML Attribute Name | Value Type                      | Description            |
|----|----|----|
| ManufacturerID     | [Hex](#user-content-attrtype-hex ) | Manufacturer ESTA ID   |
| DeviceModelID      | [Hex](#user-content-attrtype-hex ) | Unique device model ID |


</div>

As children the FTRDM has a list of `SoftwareVersionID`.

#### SoftwareVersionID
  
##### General

For each supported software version add an XML node `<SoftwareVersionID>`.
The currently defined XML attributes are specified in [table 71](#user-content-table-71 ).

<div id="table-71">

#### Table 71. *SoftwareVersionID*

| XML Attribute Name | Value Type                      | Description         |
|----|----|----|
| Value              | [Hex](#user-content-attrtype-hex ) | Software version ID |


</div>

As children the SoftwareVersionID has a list of `DMXPersonality`.

##### DMXPersonality

To define the supported software versions add an XML node
`<DMXPersonality>`. The currently defined XML attributes are specified in
[table 72](#user-content-table-72 ).

<div id="table-72">

#### Table 72. *DMXPersonality*

| XML Attribute Name | Value Type                        | Description                                                       |
|----|----|----|
| Value              | [Hex](#user-content-attrtype-hex )   | Hex Value of the DMXPersonality                                   |
| DMXMode            | [Name](#user-content-attrtype-name ) | Link to the DMX Mode that can be used with this software version. |


</div>

The DMXPersonality does not have any children.

### Art-Net Section
  
#### General

If the device supports the Art-Net protocol, this section defines the
corresponding information (XML node `<Art-Net>`).

As children the Art-Net has a list of [Maps](#user-content-map).

To define a custom mapping between Art-Net values and DMX Stream values you can add an XML node
`<Map>` as a child. The currently defined XML attributes are specified in
[table 73](#user-content-table-73 ).

By default, it is assumed that all the values are mapped 1:1, so only when you differ from that you can add a custom map.

#### Map

<div id="table-73">

#### Table 73. *Map Attributes*

| XML Attribute Name  | Value Type                            | Description                                                       |
|----|----|----|
| Key                 | [Uint](#user-content-attrtype-uint )  | Value of the Artnet value.                                        |
| Value               | [Uint](#user-content-attrtype-uint )  | Value of the DMX value.                                           |

</div>

### Streaming ACN Section
  
#### General

If the device supports the Streaming ACN protocol, this section defines the
corresponding information (XML node `<sACN>`).

As children the Streaming ACN has a list of [Maps](#user-content-map).

To define a custom mapping between Streaming ACN values and DMX Stream values you can add an XML node
`<Map>` as a child. The currently defined XML attributes are specified in
[table 73](#user-content-table-73 ).

By default, it is assumed that all the values are mapped 1:1, so only when you differ from that you can add a custom map.

### Posi Stage Net Section

This section has not yet been defined (XML node `<PosiStageNet>`).

### Open Sound Control Section

This is intentionally left empty. Future settings for custom OpenSoundControl behavior can be defined in later version. (XML node `<OpenSoundControl>`).


### CITP Section

This section has not yet been defined (XML node `<CITP>`).

## Annex A. (normative) Attribute Definitions

To describe the fixture types attributes are used. Attributes define the
function. (n) and (m) are wildcards for the enumeration of attributes
like Gobo(n) - Gobo1 and Gobo2 or VideoEffect(n)Parameter(m) -
VideoEffect1Parameter1 and VideoEffect1Parameter2. Fixture Type
Attributes without wildcards (n) or (m) are not enumerated. The
enumeration starts with 1. The currently defined Fixture Type Attributes
are specified in [table A1](#user-content-table-a1).

Note 1: The predefined Fixture Type Attributes are the preferred to use Fixture
Type Attribute. At any time user defined attributes can be introduced as well.

<div id="table-a1">

#### Table A1. *Structure of Attribute and Subattribute*

| Description                      | Attribute                                                                                                                                                                                                                                                                                                 
|----------------------------------|-----------------------------| 
| Dimmer                           | Controls the intensity of a fixture.                                                                                                                                                                                                                                                                      |
| Pan                              | Controls the fixture's sideward movement (horizontal axis).                                                                                                                                                                                                                                               |
| Tilt                             | Controls the fixture's upward and the downward movement (vertical axis).                                                                                                                                                                                                                                  |
| PanRotate                        | Controls the speed of the fixture's continuous pan movement (horizontal axis).                                                                                                                                                                                                                            |
| TiltRotate                       | Controls the speed of the fixture's continuous tilt movement (vertical axis).                                                                                                                                                                                                                             |
| PositionEffect                   | Selects the predefined position effects that are built into the fixture.                                                                                                                                                                                                                                  |
| PositionEffectRate               | Controls the speed of the predefined position effects that are built into the fixture.                                                                                                                                                                                                                    |
| PositionEffectFade               | Snaps or smooth fades with timing in running predefined position effects.                                                                                                                                                                                                                                 |
| XYZ\_X                           | Defines a fixture’s x-coordinate within an XYZ coordinate system.                                                                                                                                                                                                                                         |
| XYZ\_Y                           | Defines a fixture’s y-coordinate within an XYZ coordinate system.                                                                                                                                                                                                                                         |
| XYZ\_Z                           | Defines a fixture‘s z-coordinate within an XYZ coordinate system.                                                                                                                                                                                                                                         |
| Rot\_X                           | Defines rotation around X axis.                                                                                                                                                                                                                                                                           |
| Rot\_Y                           | Defines rotation around Y axis.                                                                                                                                                                                                                                                                           |
| Rot\_Z                           | Defines rotation around Z axis.                                                                                                                                                                                                                                                                           |
| Scale\_X                         | Scaling on X axis.                                                                                                                                                                                                                                                                                        |
| Scale\_Y                         | Scaling on Y axis.                                                                                                                                                                                                                                                                                        |
| Scale\_Z                         | Scaling on Y axis.                                                                                                                                                                                                                                                                                        |
| Scale\_XYZ                       | Unified scaling on all axis.                                                                                                                                                                                                                                                                              |
| Gobo(n)                          | The fixture’s gobo wheel (n). This is the main attribute of gobo wheel’s (n) wheel control. Selects gobos in gobo wheel (n). A different channel function sets the angle of the indexed position in the selected gobo or the angular speed of its continuous rotation.                                    |
| Gobo(n)SelectSpin                | Selects gobos whose rotation is continuous in gobo wheel (n) and controls the angular speed of the gobo’s spin within the same channel function.                                                                                                                                                          |
| Gobo(n)SelectShake               | Selects gobos which shake in gobo wheel (n) and controls the frequency of the gobo’s shake within the same channel function.                                                                                                                                                                              |
| Gobo(n)SelectEffects             | Selects gobos which run effects in gobo wheel (n).                                                                                                                                                                                                                                                        |
| Gobo(n)WheelIndex                | Controls angle of indexed rotation of gobo wheel (n).                                                                                                                                                                                                                                                     |
| Gobo(n)WheelSpin                 | Controls the speed and direction of continuous rotation of gobo wheel (n).                                                                                                                                                                                                                                |
| Gobo(n)WheelShake                | Controls frequency of the shake of gobo wheel (n).                                                                                                                                                                                                                                                        |
| Gobo(n)WheelRandom               | Controls speed of gobo wheel´s (n) random gobo slot selection.                                                                                                                                                                                                                                            |
| Gobo(n)WheelAudio                | Controls audio-controlled functionality of gobo wheel (n).                                                                                                                                                                                                                                                |
| Gobo(n)Pos                       | Controls angle of indexed rotation of gobos in gobo wheel (n). This is the main attribute of gobo wheel’s (n) wheel slot control.                                                                                                                                                                         |
| Gobo(n)PosRotate                 | Controls the speed and direction of continuous rotation of gobos in gobo wheel (n).                                                                                                                                                                                                                       |
| Gobo(n)PosShake                  | Controls frequency of the shake of gobos in gobo wheel (n).                                                                                                                                                                                                                                               |
| AnimationWheel(n)                | This is the main attribute of the animation wheel's (n) wheel control. Selects slots in the animation wheel. A different channel function sets the angle of the indexed position in the selected slot or the angular speed of its continuous rotation. Is used for animation effects with multiple slots. |
| AnimationWheel(n)Audio           | Controls audio-controlled functionality of animation wheel (n).                                                                                                                                                                                                                                           |
| AnimationWheel(n)Macro           | Selects predefined effects in animation wheel (n).                                                                                                                                                                                                                                                        |
| AnimationWheel(n)Random          | Controls frequency of animation wheel (n) random slot selection.                                                                                                                                                                                                                                          |
| AnimationWheel(n)SelectEffects   | Selects slots which run effects in animation wheel (n).                                                                                                                                                                                                                                                   |
| AnimationWheel(n)SelectShake     | Selects slots which shake in animation wheel and controls the frequency of the slots shake within the same channel function.                                                                                                                                                                              |
| AnimationWheel(n)SelectSpin      | Selects slots whose rotation is continuous in animation wheel and controls the angular speed of the slot spin within the same channel function                                                                                                                                                            |
| AnimationWheel(n)Pos             | Controls angle of indexed rotation of slots in animation wheel. This is the main attribute of animation wheel (n) wheel slot control.                                                                                                                                                                     |
| AnimationWheel(n)PosRotate       | Controls the speed and direction of continuous rotation of slots in animation wheel (n).                                                                                                                                                                                                                  |
| AnimationWheel(n)PosShake        | Controls frequency of the shake of slots in animation wheel (n).                                                                                                                                                                                                                                          |
| AnimationSystem(n)               | This is the main attribute of the animation system insertion control. Controls the insertion of the fixture's animation system in the light output. Is used for animation effects where a disk is inserted into the light output.                                                                         |
| AnimationSystem(n)Ramp           | Sets frequency of animation system (n) insertion ramp.                                                                                                                                                                                                                                                    |
| AnimationSystem(n)Shake          | Sets frequency of animation system (n) insertion shake.                                                                                                                                                                                                                                                   |
| AnimationSystem(n)Audio          | Controls audio-controlled functionality of animation system (n) insertion.                                                                                                                                                                                                                                |
| AnimationSystem(n)Random         | Controls frequency of animation system (n) random insertion.                                                                                                                                                                                                                                              |
| AnimationSystem(n)Pos            | This is the main attribute of the animation system spinning control. Controls angle of indexed rotation of animation system (n) disk.                                                                                                                                                                     |
| AnimationSystem(n)PosRotate      | Controls the speed and direction of continuous rotation of animation system (n) disk.                                                                                                                                                                                                                     |
| AnimationSystem(n)PosShake       | Controls frequency of the shake of animation system (n) disk.                                                                                                                                                                                                                                             |
| AnimationSystem(n)PosRandom      | Controls random speed of animation system (n) disk.                                                                                                                                                                                                                                                       |
| AnimationSystem(n)PosAudio       | Controls audio-controlled functionality of animation system (n) disk.                                                                                                                                                                                                                                     |
| AnimationSystem(n)Macro          | Selects predefined effects in animation system (n).                                                                                                                                                                                                                                                       |
| MediaFolder(n)                   | Selects folder that contains media content.                                                                                                                                                                                                                                                               |
| MediaContent(n)                  | Selects file with media content.                                                                                                                                                                                                                                                                          |
| ModelFolder(n)                   | Selects folder that contains 3D model content. For example 3D meshes for mapping.                                                                                                                                                                                                                         |
| ModelContent(n)                  | Selects file with 3D model content.                                                                                                                                                                                                                                                                       |
| PlayMode                         | Defines media playback mode.                                                                                                                                                                                                                                                                              |
| PlayBegin                        | Defines starting point of media content playback.                                                                                                                                                                                                                                                         |
| PlayEnd                          | Defines end point of media content playback.                                                                                                                                                                                                                                                              |
| PlaySpeed                        | Adjusts playback speed of media content.                                                                                                                                                                                                                                                                  |
| ColorEffects(n)                  | Selects predefined color effects built into the fixture.                                                                                                                                                                                                                                                  |
| Color(n)                         | The fixture’s color wheel (n). Selects colors in color wheel (n). This is the main attribute of color wheel’s (n) wheel control.                                                                                                                                                                          |
| Color(n)WheelIndex               | Controls angle of indexed rotation of color wheel (n)                                                                                                                                                                                                                                                     |
| Color(n)WheelSpin                | Controls the speed and direction of continuous rotation of color wheel (n).                                                                                                                                                                                                                               |
| Color(n)WheelRandom              | Controls frequency of color wheel´s (n) random color slot selection.                                                                                                                                                                                                                                      |
| Color(n)WheelAudio               | Controls audio-controlled functionality of color wheel (n).                                                                                                                                                                                                                                               |
| ColorAdd\_R                      | Controls the intensity of the fixture's red emitters for direct additive color mixing.                                                                                                                                                                                                                    |
| ColorAdd\_G                      | Controls the intensity of the fixture's green emitters for direct additive color mixing                                                                                                                                                                                                                   |
| ColorAdd\_B                      | Controls the intensity of the fixture's blue emitters for direct additive color mixing.                                                                                                                                                                                                                   |
| ColorAdd\_C                      | Controls the intensity of the fixture's cyan emitters for direct additive color mixing.                                                                                                                                                                                                                   |
| ColorAdd\_M                      | Controls the intensity of the fixture's magenta emitters for direct additive color mixing.                                                                                                                                                                                                                |
| ColorAdd\_Y                      | Controls the intensity of the fixture's yellow emitters for direct additive color mixing.                                                                                                                                                                                                                 |
| ColorAdd\_RY                     | Controls the intensity of the fixture's amber emitters for direct additive color mixing.                                                                                                                                                                                                                  |
| ColorAdd\_GY                     | Controls the intensity of the fixture's lime emitters for direct additive color mixing.                                                                                                                                                                                                                   |
| ColorAdd\_GC                     | Controls the intensity of the fixture's blue-green emitters for direct additive color mixing.                                                                                                                                                                                                             |
| ColorAdd\_BC                     | Controls the intensity of the fixture's light-blue emitters for direct additive color mixing.                                                                                                                                                                                                             |
| ColorAdd\_BM                     | Controls the intensity of the fixture's purple emitters for direct additive color mixing.                                                                                                                                                                                                                 |
| ColorAdd\_RM                     | Controls the intensity of the fixture's pink emitters for direct additive color mixing.                                                                                                                                                                                                                   |
| ColorAdd\_W                      | Controls the intensity of the fixture's white emitters for direct additive color mixing.                                                                                                                                                                                                                  |
| ColorAdd\_WW                     | Controls the intensity of the fixture's warm white emitters for direct additive color mixing.                                                                                                                                                                                                             |
| ColorAdd\_CW                     | Controls the intensity of the fixture's cool white emitters for direct additive color mixing.                                                                                                                                                                                                             |
| ColorAdd\_UV                     | Controls the intensity of the fixture's UV emitters for direct additive color mixing.                                                                                                                                                                                                                     |
| ColorSub\_R                      | Controls the insertion of the fixture's red filter flag for direct subtractive color mixing.                                                                                                                                                                                                              |
| ColorSub\_G                      | Controls the insertion of the fixture's green filter flag for direct subtractive color mixing.                                                                                                                                                                                                            |
| ColorSub\_B                      | Controls the insertion of the fixture's blue filter flag for direct subtractive color mixing.                                                                                                                                                                                                             |
| ColorSub\_C                      | Controls the insertion of the fixture's cyan filter flag for direct subtractive color mixing.                                                                                                                                                                                                             |
| ColorSub\_M                      | Controls the insertion of the fixture's magenta filter flag for direct subtractive color mixing.                                                                                                                                                                                                          |
| ColorSub\_Y                      | Controls the insertion of the fixture's yellow filter flag for direct subtractive color mixing.                                                                                                                                                                                                           |
| ColorMacro(n)                    | Selects predefined colors that are programed in the fixture's firmware.                                                                                                                                                                                                                                   |
| ColorMacro(n)Rate                | Controls the time between Color Macro steps.                                                                                                                                                                                                                                                              |
| CTO                              | Controls the fixture's "Correct to orange" wheel or mixing system.                                                                                                                                                                                                                                        |
| CTC                              | Controls the fixture's "Correct to color" wheel or mixing system.                                                                                                                                                                                                                                         |
| CTB                              | Controls the fixture's "Correct to blue" wheel or mixing system.                                                                                                                                                                                                                                          |
| Tint                             | Controls the fixture's "Correct green to magenta" wheel or mixing system.                                                                                                                                                                                                                                 |
| HSB\_Hue                         | Controls the fixture's color attribute regarding the hue.                                                                                                                                                                                                                                                 |
| HSB\_Saturation                  | Controls the fixture's color attribute regarding the saturation.                                                                                                                                                                                                                                          |
| HSB\_Brightness                  | Controls the fixture's color attribute regarding the brightness.                                                                                                                                                                                                                                          |
| HSB\_Quality                     | Controls the fixture's color attribute regarding the quality.                                                                                                                                                                                                                                             |
| CIE\_X                           | Controls the fixture's CIE 1931 color attribute regarding the chromaticity x.                                                                                                                                                                                                                             |
| CIE\_Y                           | Controls the fixture's CIE 1931 color attribute regarding the chromaticity y.                                                                                                                                                                                                                             |
| CIE\_Brightness                  | Controls the fixture's CIE 1931 color attribute regarding the brightness (Y).                                                                                                                                                                                                                             |
| ColorRGB\_Red                    | Controls the fixture's red attribute for indirect RGB color mixing.                                                                                                                                                                                                                                       |
| ColorRGB\_Green                  | Controls the fixture's green attribute for indirect RGB color mixing.                                                                                                                                                                                                                                     |
| ColorRGB\_Blue                   | Controls the fixture's blue attribute for indirect RGB color mixing.                                                                                                                                                                                                                                      |
| ColorRGB\_Cyan                   | Controls the fixture's cyan attribute for indirect CMY color mixing.                                                                                                                                                                                                                                      |
| ColorRGB\_Magenta                | Controls the fixture's magenta attribute for indirect CMY color mixing.                                                                                                                                                                                                                                   |
| ColorRGB\_Yellow                 | Controls the fixture's yellow attribute for indirect CMY color mixing.                                                                                                                                                                                                                                    |
| ColorRGB\_Quality                | Controls the fixture's quality attribute for indirect color mixing.                                                                                                                                                                                                                                       |
| VideoBoost\_R                    | Adjusts color boost red of content.                                                                                                                                                                                                                                                                       |
| VideoBoost\_G                    | Adjusts color boost green of content.                                                                                                                                                                                                                                                                     |
| VideoBoost\_B                    | Adjusts color boost blue of content.                                                                                                                                                                                                                                                                      |
| VideoHueShift                    | Adjusts color hue shift of content.                                                                                                                                                                                                                                                                       |
| VideoSaturation                  | Adjusts saturation of content.                                                                                                                                                                                                                                                                            |
| VideoBrightness                  | Adjusts brightness of content.                                                                                                                                                                                                                                                                            |
| VideoContrast                    | Adjusts contrast of content.                                                                                                                                                                                                                                                                              |
| VideoKeyColor\_R                 | Adjusts red color for color keying.                                                                                                                                                                                                                                                                       |
| VideoKeyColor\_G                 | Adjusts green color for color keying.                                                                                                                                                                                                                                                                     |
| VideoKeyColor\_B                 | Adjusts blue color for color keying.                                                                                                                                                                                                                                                                      |
| VideoKeyIntensity                | Adjusts intensity of color keying.                                                                                                                                                                                                                                                                        |
| VideoKeyTolerance                | Adjusts tolerance of color keying.                                                                                                                                                                                                                                                                        |
| StrobeDuration                   | Controls the length of a strobe flash.                                                                                                                                                                                                                                                                    |
| StrobeRate                       | Controls the time between strobe flashes.                                                                                                                                                                                                                                                                 |
| StrobeFrequency                  | Controls the frequency of strobe flashes.                                                                                                                                                                                                                                                                 |
| StrobeModeShutter                | Strobe mode shutter. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                         |
| StrobeModeStrobe                 | Strobe mode strobe. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                          |
| StrobeModePulse                  | Strobe mode pulse. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                           |
| StrobeModePulseOpen              | Strobe mode opening pulse. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                   |
| StrobeModePulseClose             | Strobe mode closing pulse. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                   |
| StrobeModeRandom                 | Strobe mode random strobe. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                   |
| StrobeModeRandomPulse            | Strobe mode random pulse. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                                    |
| StrobeModeRandomPulseOpen        | Strobe mode random opening pulse. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                            |
| StrobeModeRandomPulseClose       | Strobe mode random closing pulse. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                            |
| StrobeModeEffect                 | Strobe mode random shutter effect feature. Use this attribute together with StrobeFrequency to define the type of the shutter / strobe.                                                                                                                                                                   |
| Shutter(n)                       | Controls the fixture´s mechanical or electronical shutter feature.                                                                                                                                                                                                                                        |
| Shutter(n)Strobe                 | Controls the frequency of the fixture´s mechanical or electronical strobe shutter feature.                                                                                                                                                                                                                |
| Shutter(n)StrobePulse            | Controls the frequency of the fixture´s mechanical or electronical pulse shutter feature.                                                                                                                                                                                                                 |
| Shutter(n)StrobePulseClose       | Controls the frequency of the fixture´s mechanical or electronical closing pulse shutter feature. The pulse is described by a ramp function.                                                                                                                                                              |
| Shutter(n)StrobePulseOpen        | Controls the frequency of the fixture´s mechanical or electronical opening pulse shutter feature. The pulse is described by a ramp function.                                                                                                                                                              |
| Shutter(n)StrobeRandom           | Controls the frequency of the fixture´s mechanical or electronical random strobe shutter feature.                                                                                                                                                                                                         |
| Shutter(n)StrobeRandomPulse      | Controls the frequency of the fixture´s mechanical or electronical random pulse shutter feature.                                                                                                                                                                                                          |
| Shutter(n)StrobeRandomPulseClose | Controls the frequency of the fixture´s mechanical or electronical random closing pulse shutter feature. The pulse is described by a ramp function.                                                                                                                                                       |
| Shutter(n)StrobeRandomPulseOpen  | Controls the frequency of the fixture´s mechanical or electronical random opening pulse shutter feature. The pulse is described by a ramp function.                                                                                                                                                       |
| Shutter(n)StrobeEffect           | Controls the frequency of the fixture´s mechanical or electronical shutter effect feature.                                                                                                                                                                                                                |
| Iris                             | Controls the diameter of the fixture's beam.                                                                                                                                                                                                                                                              |
| IrisStrobe                       | Sets frequency of the iris's strobe feature.                                                                                                                                                                                                                                                              |
| IrisStrobeRandom                 | Sets frequency of the iris's random movement.                                                                                                                                                                                                                                                             |
| IrisPulseClose                   | Sets frequency of iris's closing pulse.                                                                                                                                                                                                                                                                   |
| IrisPulseOpen                    | Sets frequency of iris's opening pulse.                                                                                                                                                                                                                                                                   |
| IrisRandomPulseClose             | Sets frequency of iris's random closing pulse.                                                                                                                                                                                                                                                            |
| IrisRandomPulseOpen              | Sets frequency of iris's random opening pulse.                                                                                                                                                                                                                                                            |
| Frost(n)                         | The ability to soften the fixture's spot light with a frosted lens.                                                                                                                                                                                                                                       |
| Frost(n)PulseOpen                | Sets frequency of frost's opening pulse                                                                                                                                                                                                                                                                   |
| Frost(n)PulseClose               | Sets frequency of frost's closing pulse.                                                                                                                                                                                                                                                                  |
| Frost(n)Ramp                     | Sets frequency of frost's ramp.                                                                                                                                                                                                                                                                           |
| Prism(n)                         | The fixture’s prism wheel (n). Selects prisms in prism wheel (n). A different channel function sets the angle of the indexed position in the selected prism or the angular speed of its continuous rotation. This is the main attribute of prism wheel’s (n) wheel control.                               |
| Prism(n)SelectSpin               | Selects prisms whose rotation is continuous in prism wheel (n) and controls the angular speed of the prism’s spin within the same channel function.                                                                                                                                                       |
| Prism(n)Macro                    | Macro functions of prism wheel (n).                                                                                                                                                                                                                                                                       |
| Prism(n)Pos                      | Controls angle of indexed rotation of prisms in prism wheel (n). This is the main attribute of prism wheel’s 1 wheel slot control.                                                                                                                                                                        |
| Prism(n)PosRotate                | Controls the speed and direction of continuous rotation of prisms in prism wheel (n).                                                                                                                                                                                                                     |
| Effects(n)                       | Generically predefined macros and effects of a fixture.                                                                                                                                                                                                                                                   |
| Effects(n)Rate                   | Frequency of running effects.                                                                                                                                                                                                                                                                             |
| Effects(n)Fade                   | Snapping or smooth look of running effects.                                                                                                                                                                                                                                                               |
| Effects(n)Adjust(m)              | Controls parameter (m) of effect (n)                                                                                                                                                                                                                                                                      |
| Effects(n)Pos                    | Controls angle of indexed rotation of slot/effect in effect wheel/macro (n). This is the main attribute of effect wheel/macro (n) slot/effect control.                                                                                                                                                    |
| Effects(n)PosRotate              | Controls speed and direction of slot/effect in effect wheel (n).                                                                                                                                                                                                                                          |
| EffectsSync                      | Sets offset between running effects and effects 2.                                                                                                                                                                                                                                                        |
| BeamShaper                       | Activates fixture's beam shaper.                                                                                                                                                                                                                                                                          |
| BeamShaperMacro                  | Predefined presets for fixture's beam shaper positions.                                                                                                                                                                                                                                                   |
| BeamShaperPos                    | Indexing of fixture's beam shaper.                                                                                                                                                                                                                                                                        |
| BeamShaperPosRotate              | Continuous rotation of fixture's beam shaper.                                                                                                                                                                                                                                                             |
| Zoom                             | Controls the spread of the fixture's beam/spot.                                                                                                                                                                                                                                                           |
| ZoomModeSpot                     | Selects spot mode of zoom.                                                                                                                                                                                                                                                                                |
| ZoomModeBeam                     | Selects beam mode of zoom.                                                                                                                                                                                                                                                                                |
| DigitalZoom                      | Controls the image size within the defined projection. Used on digital projection based devices                                                                                                                                                                                                           |
| Focus(n)                         | Controls the sharpness of the fixture's spot light. Can blur or sharpen the edge of the spot.                                                                                                                                                                                                             |
| Focus(n)Adjust                   | Autofocuses functionality using presets.                                                                                                                                                                                                                                                                  |
| Focus(n)Distance                 | Autofocuses functionality using distance.                                                                                                                                                                                                                                                                 |
| Control(n)                       | Controls the channel of a fixture.                                                                                                                                                                                                                                                                        |
| DimmerMode                       | Selects different modes of intensity.                                                                                                                                                                                                                                                                     |
| DimmerCurve                      | Selects different dimmer curves of the fixture.                                                                                                                                                                                                                                                           |
| BlackoutMode                     | Close the light output under certain conditions (movement correction, gobo movement, etc.).                                                                                                                                                                                                               |
| LEDFrequency                     | Controls LED frequency.                                                                                                                                                                                                                                                                                   |
| LEDZoneMode                      | Changes zones of LEDs.                                                                                                                                                                                                                                                                                    |
| PixelMode                        | Controls behavior of LED pixels.                                                                                                                                                                                                                                                                          |
| PanMode                          | Selects fixture's pan mode. Selects between a limited pan range (e.g. -270 to 270) or a continuous pan range.                                                                                                                                                                                             |
| TiltMode                         | Selects fixture's pan mode. Selects between a limited tilt range (e.g. -130 to 130) or a continuous tilt range.                                                                                                                                                                                           |
| PanTiltMode                      | Selects fixture's pan/tilt mode. Selects between a limited pan/tilt range or a continuous pan/tilt range.                                                                                                                                                                                                 |
| PositionModes                    | Selects the fixture's position mode.                                                                                                                                                                                                                                                                      |
| Gobo(n)WheelMode                 | Changes control between selecting, indexing, and rotating the gobos of gobo wheel (n).                                                                                                                                                                                                                    |
| GoboWheelShortcutMode            | Defines whether the gobo wheel takes the shortest distance between two positions.                                                                                                                                                                                                                         |
| AnimationWheel(n)Mode            | Changes control between selecting, indexing, and rotating the slots of animation wheel (n).                                                                                                                                                                                                               |
| AnimationWheelShortcutMode       | Defines whether the animation wheel takes the shortest distance between two positions.                                                                                                                                                                                                                    |
| Color(n)Mode                     | Changes control between selecting, continuous selection, half selection, random selection, color spinning, etc. in colors of color wheel (n).                                                                                                                                                             |
| ColorWheelShortcutMode           | Defines whether the color wheel takes the shortest distance between two colors.                                                                                                                                                                                                                           |
| CyanMode                         | Controls how Cyan is used within the fixture's cyan CMY-mixing feature.                                                                                                                                                                                                                                   |
| MagentaMode                      | Controls how Cyan is used within the fixture's magenta CMY-mixing.                                                                                                                                                                                                                                        |
| YellowMode                       | Controls how Cyan is used within the fixture's yellow CMY-mixing feature.                                                                                                                                                                                                                                 |
| ColorMixMode                     | Changes control between selecting continuous selection, half selection, random selection, color spinning, etc. in color mixing.                                                                                                                                                                           |
| ChromaticMode                    | Selects chromatic behavior of the device.                                                                                                                                                                                                                                                                 |
| ColorCalibrationMode             | Sets calibration mode (for example on/off).                                                                                                                                                                                                                                                               |
| ColorConsistency                 | Controls consistent behavior of color.                                                                                                                                                                                                                                                                    |
| ColorControl                     | Controls special color related functions.                                                                                                                                                                                                                                                                 |
| ColorModelMode                   | Controls color model (CMY/RGB/HSV...).                                                                                                                                                                                                                                                                     |
| ColorSettingsReset               | Resets settings of color control channel.                                                                                                                                                                                                                                                                 |
| ColorUniformity                  | Controls behavior of color uniformity.                                                                                                                                                                                                                                                                    |
| CRIMode                          | Controls CRI settings of output.                                                                                                                                                                                                                                                                          |
| CustomColor                      | Custom color related functions (save, recall..).                                                                                                                                                                                                                                                          |
| UVStability                      | Settings for UV stability color behavior.                                                                                                                                                                                                                                                                 |
| WavelengthCorrection             | Settings for Wavelength correction of colors.                                                                                                                                                                                                                                                            |
| WhiteCount                       | Controls if White LED is proportionally added to RGB.                                                                                                                                                                                                                                                     |
| StrobeMode                       | Changes strobe style - strobe, pulse, random strobe, etc. - of the shutter attribute.                                                                                                                                                                                                                     |
| ZoomMode                         | Changes modes of the fixture´s zoom.                                                                                                                                                                                                                                                                      |
| FocusMode                        | Changes modes of the fixture’s focus - manual or auto- focus.                                                                                                                                                                                                                                             |
| IrisMode                         | Changes modes of the fixture’s iris - linear, strobe, pulse.                                                                                                                                                                                                                                              |
| Fan(n)Mode                       | Controls fan (n) mode.                                                                                                                                                                                                                                                                                    |
| FollowSpotMode                   | Selects follow spot control mode.                                                                                                                                                                                                                                                                         |
| BeamEffectIndexRotateMode        | Changes mode to control either index or rotation of the beam effects.                                                                                                                                                                                                                                     |
| IntensityMSpeed                  | Movement speed of the fixture's intensity.                                                                                                                                                                                                                                                                |
| PositionMSpeed                   | Movement speed of the fixture's pan/tilt.                                                                                                                                                                                                                                                                 |
| ColorMixMSpeed                   | Movement speed of the fixture's ColorMix presets.                                                                                                                                                                                                                                                         |
| ColorWheelSelectMSpeed           | Movement speed of the fixture's color wheel.                                                                                                                                                                                                                                                              |
| GoboWheel(n)MSpeed               | Movement speed of the fixture's gobo wheel (n).                                                                                                                                                                                                                                                           |
| IrisMSpeed                       | Movement speed of the fixture's iris.                                                                                                                                                                                                                                                                     |
| Prism(n)MSpeed                   | Movement speed of the fixture's prism wheel (n).                                                                                                                                                                                                                                                          |
| FocusMSpeed                      | Movement speed of the fixture's focus.                                                                                                                                                                                                                                                                    |
| Frost(n)MSpeed                   | Movement speed of the fixture's frost (n).                                                                                                                                                                                                                                                                |
| ZoomMSpeed                       | Movement speed of the fixture's zoom.                                                                                                                                                                                                                                                                     |
| FrameMSpeed                      | Movement speed of the fixture's shapers.                                                                                                                                                                                                                                                                  |
| GlobalMSpeed                     | General speed of fixture's features.                                                                                                                                                                                                                                                                      |
| ReflectorAdjust                  | Movement speed of the fixture's frost.                                                                                                                                                                                                                                                                    |
| FixtureGlobalReset               | Generally resets the entire fixture.                                                                                                                                                                                                                                                                      |
| DimmerReset                      | Resets the fixture's dimmer.                                                                                                                                                                                                                                                                              |
| ShutterReset                     | Resets the fixture's shutter.                                                                                                                                                                                                                                                                             |
| BeamReset                        | Resets the fixture's beam features.                                                                                                                                                                                                                                                                       |
| ColorMixReset                    | Resets the fixture's color mixing system.                                                                                                                                                                                                                                                                 |
| ColorWheelReset                  | Resets the fixture's color wheel.                                                                                                                                                                                                                                                                         |
| FocusReset                       | Resets the fixture's focus.                                                                                                                                                                                                                                                                               |
| FrameReset                       | Resets the fixture's shapers.                                                                                                                                                                                                                                                                             |
| GoboWheelReset                   | Resets the fixture's gobo wheel.                                                                                                                                                                                                                                                                          |
| IntensityReset                   | Resets the fixture's intensity.                                                                                                                                                                                                                                                                           |
| IrisReset                        | Resets the fixture's iris.                                                                                                                                                                                                                                                                                |
| PositionReset                    | Resets the fixture's pan/tilt.                                                                                                                                                                                                                                                                            |
| PanReset                         | Resets the fixture's pan.                                                                                                                                                                                                                                                                                 |
| TiltReset                        | Resets the fixture's tilt.                                                                                                                                                                                                                                                                                |
| ZoomReset                        | Resets the fixture's zoom.                                                                                                                                                                                                                                                                                |
| CTBReset                         | Resets the fixture's CTB.                                                                                                                                                                                                                                                                                 |
| CTOReset                         | Resets the fixture's CTO.                                                                                                                                                                                                                                                                                 |
| CTCReset                         | Resets the fixture's CTC.                                                                                                                                                                                                                                                                                 |
| AnimationSystemReset             | Resets the fixture's animation system features.                                                                                                                                                                                                                                                           |
| FixtureCalibrationReset          | Resets the fixture's calibration.                                                                                                                                                                                                                                                                         |
| Function                         | Generally controls features of the fixture.                                                                                                                                                                                                                                                               |
| LampControl                      | Controls the fixture's lamp on/lamp off feature.                                                                                                                                                                                                                                                          |
| DisplayIntensity                 | Adjusts intensity of display                                                                                                                                                                                                                                                                              |
| DMXInput                         | Selects DMX Input                                                                                                                                                                                                                                                                                         |
| NoFeature                        | Ranges without a functionality.                                                                                                                                                                                                                                                                           |
| Blower(n)                        | Fog or hazer‘s blower feature.                                                                                                                                                                                                                                                                            |
| Fan(n)                           | Fog or hazer's Fan feature.                                                                                                                                                                                                                                                                               |
| Fog(n)                           | Fog or hazer's Fog feature.                                                                                                                                                                                                                                                                               |
| Haze(n)                          | Fog or hazer's Haze feature.                                                                                                                                                                                                                                                                              |
| LampPowerMode                    | Controls the energy consumption of the lamp.                                                                                                                                                                                                                                                              |
| Fans                             | Controls a fixture or device fan.                                                                                                                                                                                                                                                                          |
| Blade(n)A                        | 1 of 2 shutters that shape the top/right/bottom/left of the beam.                                                                                                                                                                                                                                         |
| Blade(n)B                        | 2 of 2 shutters that shape the top/right/bottom/left of the beam.                                                                                                                                                                                                                                         |
| Blade(n)Rot                      | Rotates position of blade(n).                                                                                                                                                                                                                                                                             |
| ShaperRot                        | Rotates position of blade assembly.                                                                                                                                                                                                                                                                       |
| ShaperMacros                     | Predefined presets for shaper positions.                                                                                                                                                                                                                                                                  |
| ShaperMacrosSpeed                | Speed of predefined effects on shapers.                                                                                                                                                                                                                                                                   |
| BladeSoft(n)A                    | 1 of 2 soft edge blades that shape the top/right/bottom/left of the beam.                                                                                                                                                                                                                                 |
| BladeSoft(n)B                    | 2 of 2 soft edge blades that shape the top/right/bottom/left of the beam.                                                                                                                                                                                                                                 |
| KeyStone(n)A                     | 1 of 2 corners that shape the top/right/bottom/left of the beam.                                                                                                                                                                                                                                          |
| KeyStone(n)B                     | 2 of 2 corners that shape the top/right/bottom/left of the beam.                                                                                                                                                                                                                                          |
| Video                            | Controls video features.                                                                                                                                                                                                                                                                                  |
| VideoEffect(n)Type               | Selects dedicated effects which are used for media.                                                                                                                                                                                                                                                       |
| VideoEffect(n)Parameter(m)       | Controls parameter (m) of VideoEffect(n)Type.                                                                                                                                                                                                                                                             |
| VideoCamera(n)                   | Selects the video camera(n).                                                                                                                                                                                                                                                                              |
| VideoSoundVolume(n)              | Adjusts sound volume.                                                                                                                                                                                                                                                                                     |
| VideoBlendMode                   | Defines mode of video blending.                                                                                                                                                                                                                                                                           |
| InputSource                      | Defines media input source e.g. a camera input.                                                                                                                                                                                                                                                           |
| FieldOfView                      | Defines field of view.                                                                                                                                                                                                                                                                                    |

</div>


## Annex B (normative) Attribute Listing

(n) and (m) are wildcards for enumeration of attributes e.g., Gobo(n) -
Gobo1 and Gobo2 or VideoEffect(n)Parameter(m) - VideoEffect1Parameter1
and VideoEffect1Parameter2. Attributes without the wildcards (n) or (m)
are not enumerated. The enumeration starts with 1. Attributes names are
considered as normalized. The upper and lower case of attribute names is
not taken into account.

```
   <AttributeDefinitions>  
       <ActivationGroups>  
           <ActivationGroup  Name="PanTilt" />  
           <ActivationGroup  Name="XYZ" />  
           <ActivationGroup  Name="Rot_XYZ" />
           <ActivationGroup  Name="Scale_XYZ" />
           <ActivationGroup  Name="ColorRGB" />  
           <ActivationGroup  Name="ColorHSB" />  
           <ActivationGroup  Name="ColorCIE" />  
           <ActivationGroup  Name="ColorIndirect" />  
           <ActivationGroup  Name="Gobo(n)" />  
           <ActivationGroup  Name="Gobo(n)Pos" />  
           <ActivationGroup  Name="AnimationWheel(n)" />  
           <ActivationGroup  Name="AnimationWheel(n)Pos" />  
           <ActivationGroup  Name="AnimationSystem(n)" />  
           <ActivationGroup  Name="AnimationSystem(n)Pos" />  
           <ActivationGroup  Name="Prism" />  
           <ActivationGroup  Name="BeamShaper" />  
           <ActivationGroup  Name="Shaper" />  
       </ActivationGroups>  
       <FeatureGroups>  
           <FeatureGroup  Name="Dimmer">  
               <Feature  Name="Dimmer" />  
           </FeatureGroup>  
           <FeatureGroup  Name="Position">  
               <Feature  Name="PanTilt" />  
               <Feature  Name="XYZ" />  
               <Feature  Name="Rotation"/>  
               <Feature  Name="Scale"/>  
           </FeatureGroup>  
           <FeatureGroup  Name="Gobo">  
               <Feature  Name="Gobo" />  
               <Feature  Name="Media"/>  
           </FeatureGroup>  
           <FeatureGroup  Name="Color">  
               <Feature  Name="Color" />  
               <Feature  Name="RGB" />  
               <Feature  Name="HSB" />  
               <Feature  Name="CIE" />  
               <Feature  Name="Indirect" />  
               <Feature Name="ColorCorrection"/>  
               <Feature Name="HSBC_Shift"/>  
               <Feature Name="ColorKey"/>  
           </FeatureGroup>  
           <FeatureGroup  Name="Beam">  
               <Feature  Name="Beam" />  
           </FeatureGroup>  
           <FeatureGroup  Name="Focus">  
               <Feature  Name="Focus" />  
           </FeatureGroup>  
           <FeatureGroup  Name="Control">  
               <Feature  Name="Control" />  
           </FeatureGroup>  
           <FeatureGroup  Name="Shapers">  
               <Feature  Name="Shapers" />  
           </FeatureGroup>  
           <FeatureGroup  Name="Video">  
               <Feature  Name="Video" />  
           </FeatureGroup>  
       </FeatureGroups>  
       <Attributes>  
           <Attribute Name="Dimmer" Pretty="Dim" Feature="Dimmer.Dimmer" />  
           <Attribute Name="Pan" Pretty="P" ActivationGroup="PanTilt" Feature="Position.PanTilt" PhysicalUnit="Angle" />  
           <Attribute Name="Tilt" Pretty="T" ActivationGroup="PanTilt" Feature="Position.PanTilt" PhysicalUnit="Angle" />  
           <Attribute Name="PanRotate" Pretty="P Rotate" Feature="Position.PanTilt" PhysicalUnit="AngularSpeed" />  
           <Attribute Name="TiltRotate" Pretty="T Rotate" Feature="Position.PanTilt" PhysicalUnit="AngularSpeed" />  
           <Attribute Name="PositionEffect" Pretty="Pos FX" Feature="Position.PanTilt" />  
           <Attribute Name="PositionEffectRate" Pretty="Pos FX Rate" Feature="Position.PanTilt" />  
           <Attribute Name="PositionEffectFade" Pretty="Pos FX Fade" Feature="Position.PanTilt" />  
           <Attribute Name="XYZ_X" Pretty="X" ActivationGroup="XYZ" Feature="Position.XYZ" PhysicalUnit="Length" />  
           <Attribute Name="XYZ_Y" Pretty="Y" ActivationGroup="XYZ" Feature="Position.XYZ" PhysicalUnit="Length" />  
           <Attribute Name="XYZ_Z" Pretty="Z" ActivationGroup="XYZ" Feature="Position.XYZ" PhysicalUnit="Length" /  
           <Attribute Name="Rot_X" Pretty="Rot X" ActivationGroup="Rot_XYZ" Feature="Position.Rotation" PhysicalUnit="Angle" />  
           <Attribute Name="Rot_Y" Pretty="Rot Y" ActivationGroup="Rot_XYZ" Feature="Position.Rotation" PhysicalUnit="Angle" />  
           <Attribute Name="Rot_Z" Pretty="Rot Z" ActivationGroup="Rot_XYZ" Feature="Position.Rotation" PhysicalUnit="Angle" />  
           <Attribute Name="Scale_X" Pretty="Scale X" ActivationGroup="Scale_XYZ" Feature="Position.Scale" PhysicalUnit="Percent" />  
           <Attribute Name="Scale_Y" Pretty="Scale Y" ActivationGroup="Scale_XYZ" Feature="Position.Scale" PhysicalUnit="Percent" />  
           <Attribute Name="Scale_Z" Pretty="Scale Z" ActivationGroup="Scale_XYZ" Feature="Position.Scale" PhysicalUnit="Percent" />  
           <Attribute Name="Scale_XYZ" Pretty="Scale XYZ" ActivationGroup="Scale_XYZ" Feature="Position.Scale" PhysicalUnit="Percent" />  
           <Attribute Name="Gobo(n)" Pretty="G(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Gobo(n)SelectSpin" Pretty="Select Spin" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" PhysicalUnit="AngularSpeed" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Gobo(n)SelectShake" Pretty="Select Shake" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" PhysicalUnit="Frequency" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
              <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="20" PhysicalTo="20"/> This is the amount of shake as a percentage of the image size and defines the peak amplitude of the shake
           <Attribute Name="Gobo(n)SelectEffects" Pretty="Select Effects" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" />  
           <Attribute Name="Gobo(n)WheelIndex" Pretty="Wheel Index" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" PhysicalUnit="Angle" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Gobo(n)WheelSpin" Pretty="Wheel Spin" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" PhysicalUnit="AngularSpeed" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Gobo(n)WheelShake" Pretty="Wheel Shake" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" PhysicalUnit="Frequency" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
              <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="20" PhysicalTo="20"/> This is the amount of shake as a percentage of the image size and defines the peak amplitude of the shake
           <Attribute Name="Gobo(n)WheelRandom" Pretty="Wheel Random" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Gobo(n)WheelAudio" Pretty="Wheel Audio" MainAttribute="Gobo(n)" ActivationGroup="Gobo(n)" Feature="Gobo.Gobo" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Gobo(n)Pos" Pretty="G(n) &lt;&gt;" ActivationGroup="Gobo(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="Angle" />  
           <Attribute Name="Gobo(n)PosRotate" Pretty="Rotate" MainAttribute="Gobo(n)Pos" ActivationGroup="Gobo(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="AngularSpeed" />  
           <Attribute Name="Gobo(n)PosShake" Pretty="Shake" MainAttribute="Gobo(n)Pos" ActivationGroup="Gobo(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="Frequency" />
              <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="20" PhysicalTo="20"/> This defines the peak amplitude of the shake
           <Attribute Name="AnimationWheel(n)" ActivationGroup="AnimationWheel(n)" Pretty="Anim(n)" Feature="Gobo.Gobo" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="AnimationWheel(n)Audio" ActivationGroup="AnimationWheel(n)" MainAttribute="AnimationWheel(n)" Feature="Gobo.Gobo" Pretty="Anim Audio" /> 
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="AnimationWheel(n)Macro" ActivationGroup="AnimationWheel(n)" MainAttribute="AnimationWheel(n)" Feature="Gobo.Gobo" Pretty="Anim FX" />
           <Attribute Name="AnimationWheel(n)Random" ActivationGroup="AnimationWheel(n)" MainAttribute="AnimationWheel(n)" Feature="Gobo.Gobo" PhysicalUnit="Frequency" Pretty="Anim Random" /> 
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="AnimationWheel(n)SelectEffects" ActivationGroup="AnimationWheel(n)" MainAttribute="AnimationWheel(n)" Feature="Gobo.Gobo" Pretty="Anim Select FX" />  
           <Attribute Name="AnimationWheel(n)SelectShake" ActivationGroup="AnimationWheel(n)" MainAttribute="AnimationWheel(n)" Feature="Gobo.Gobo" PhysicalUnit="Frequency" Pretty="Anim Select Shake" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
              <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="20" PhysicalTo="20"/> This defines the peak amplitude of the shake
           <Attribute Name="AnimationWheel(n)SelectSpin" ActivationGroup="AnimationWheel(n)" MainAttribute="AnimationWheel(n)" Feature="Gobo.Gobo" PhysicalUnit="AngularSpeed" Pretty="Anim Select Spin" />
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="AnimationWheel(n)Pos" ActivationGroup="AnimationWheel(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="Angle" Pretty="Anim Pos" />  
           <Attribute Name="AnimationWheel(n)PosRotate" ActivationGroup="AnimationWheel(n)Pos" MainAttribute="AnimationWheel(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="AngularSpeed" Pretty="Anim Rotate" />  
           <Attribute Name="AnimationWheel(n)PosShake" ActivationGroup="AnimationWheel(n)Pos" MainAttribute="AnimationWheel(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="Frequency" Pretty="Anim Shake" />  
              <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="20" PhysicalTo="20"/> This defines the peak amplitude of the shake
           <Attribute Name="AnimationSystem(n)" ActivationGroup="AnimationSystem(n)" Feature="Gobo.Gobo" PhysicalUnit="Percent" Pretty="Anim System"/>  
           <Attribute Name="AnimationSystem(n)Ramp" ActivationGroup="AnimationSystem(n)" MainAttribute="AnimationSystem(n)" "Feature="Gobo.Gobo" PhysicalUnit="Frequency" Pretty="Anim System Ramp"/>  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="50" PhysicalTo="50"/> This defines the duration of the ramp in relation to the period.
              <SubPhysicalUnit Type="AmplitudeMin" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum position in relation to the whole way of the spline
              <SubPhysicalUnit Type="AmplitudeMax" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the maximum position in relation to the whole way of the spline
           <Attribute Name="AnimationSystem(n)Shake" ActivationGroup="AnimationSystem(n)" MainAttribute="AnimationSystem(n)" Feature="Gobo.Gobo" PhysicalUnit="Frequency" Pretty="Anim System Shake/>
              <SubPhysicalUnit Type="AmplitudeMin" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum position in relation to the whole way of the spline
              <SubPhysicalUnit Type="AmplitudeMax" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the maximum position in relation to the whole way of the spline
           <Attribute Name="AnimationSystem(n)Audio" ActivationGroup="AnimationSystem(n)" MainAttribute="AnimationSystem(n)" Feature="Gobo.Gobo" PhysicalUnit="None" Pretty="Anim System Audio/>  
           <Attribute Name="AnimationSystem(n)Random" ActivationGroup="AnimationSystem(n)" MainAttribute="AnimationSystem(n)" Feature="Gobo.Gobo" PhysicalUnit="None" Pretty="Anim System Random/>  
           <Attribute Name="AnimationSystem(n)Pos" ActivationGroup="AnimationSystem(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="Angle" Pretty="Anim System Pos"/>  
           <Attribute Name="AnimationSystem(n)PosRotate" ActivationGroup="AnimationSystem(n)Pos" MainAttribute="AnimationSystem(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="AngularSpeed" Pretty="Anim System Rotate"/>  
           <Attribute Name="AnimationSystem(n)PosShake" ActivationGroup="AnimationSystem(n)Pos" MainAttribute="AnimationSystem(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="Frequency" Pretty="Anim System Shake"/>  
              <SubPhysicalUnit Type="Amplitude" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the peak amplitude of the shake
           <Attribute Name="AnimationSystem(n)PosRandom" ActivationGroup="AnimationSystem(n)Pos" MainAttribute="AnimationSystem(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="None" Pretty="Anim System Rot Random"/>  
           <Attribute Name="AnimationSystem(n)PosAudio" ActivationGroup="AnimationSystem(n)Pos" MainAttribute="AnimationSystem(n)Pos" Feature="Gobo.Gobo" PhysicalUnit="None" Pretty="Anim System Rot Audio"/>  
           <Attribute Name="AnimationSystem(n)Macro" Feature="Gobo.Gobo" PhysicalUnit="None" Pretty="Anim System Macro"/>  
           <Attribute Name="MediaFolder(n)" Pretty="Media Folder(n)" Feature="Gobo.Media" />  
           <Attribute Name="MediaContent(n)" Pretty="Media Content(n)" Feature="Gobo.Media" />  
           <Attribute Name="ModelFolder(n)" Pretty="Model Folder(n)" Feature="Gobo.Media" PhysicalUnit="None" />  
           <Attribute Name="ModelContent(n)" Pretty="Model Content(n)" Feature="Gobo.Media" PhysicalUnit="None" />  
           <Attribute Name="Playmode" Pretty="Playmode" Feature="Gobo.Media" />  
           <Attribute Name="PlayBegin" Pretty="Play Begin" Feature="Gobo.Media" PhysicalUnit="Time" />  
           <Attribute Name="PlayEnd" Pretty="Play End" Feature="Gobo.Media" PhysicalUnit="Time" />  
           <Attribute Name="PlaySpeed" Pretty="Play Speed" Feature="Gobo.Media" PhysicalUnit="Percent" />  
           <Attribute Name="ColorEffects(n)" Pretty="Color FX(n)" Feature="Color.Color" />  
           <Attribute Name="Color(n)" Pretty="C(n)" ActivationGroup="ColorRGB" Feature="Color.Color" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Color(n)WheelIndex" Pretty="Wheel Index" MainAttribute="Color(n)" ActivationGroup="ColorRGB" Feature="Color.Color" PhysicalUnit="Angle" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Color(n)WheelSpin" Pretty="Wheel Spin" MainAttribute="Color(n)" ActivationGroup="ColorRGB" Feature="Color.Color" PhysicalUnit="AngularSpeed" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Color(n)WheelRandom" Pretty="Wheel Random" MainAttribute="Color(n)" ActivationGroup="ColorRGB" Feature="Color.Color" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="Color(n)WheelAudio" Pretty="Wheel Audio" MainAttribute="Color(n)" ActivationGroup="ColorRGB" Feature="Color.Color" />  
              <SubPhysicalUnit Type="PlacementOffset" PhysicalUnit="Degree" PhysicalFrom="270" PhysicalTo="270"/>
           <Attribute Name="ColorAdd_R" Pretty="R" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.64,0.33,21.3" />  
           <Attribute Name="ColorAdd_G" Pretty="G" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.3,0.6,71.5" />  
           <Attribute Name="ColorAdd_B" Pretty="B" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.15,0.06,7.2" />  
           <Attribute Name="ColorAdd_C" Pretty="C" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.225,0.329,78.7" />  
           <Attribute Name="ColorAdd_M" Pretty="M" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.321,0.154,28.5" />  
           <Attribute Name="ColorAdd_Y" Pretty="Y" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.419,0.505,92.8" />  
           <Attribute Name="ColorAdd_RY" Pretty="Amber" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.477,0.460,57.0" />  
           <Attribute Name="ColorAdd_GY" Pretty="Lime" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.372,0.543,82.1" />  
           <Attribute Name="ColorAdd_GC" Pretty="Blue-Green" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.250,0.419,75.1" />  
           <Attribute Name="ColorAdd_BC" Pretty="Light-Blue " ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.200,0.239,43.0" />  
           <Attribute Name="ColorAdd_BM" Pretty="Purple" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.254,0.117,17.9" />  
           <Attribute Name="ColorAdd_RM" Pretty="Pink" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.403,0.200,24.9" />  
           <Attribute Name="ColorAdd_W" Pretty="White" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.313,0.329,100.0" />  
           <Attribute Name="ColorAdd_WW" Pretty="WW" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.319,0.340,99.3" />  
           <Attribute Name="ColorAdd_CW" Pretty="CW" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.306,0.329,97.9" />  
           <Attribute Name="ColorAdd_UV" Pretty="UV" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.176,0.005,0.6"/>  
           <Attribute Name="ColorSub_R" Pretty="R" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.64,0.33,21.3" />  
           <Attribute Name="ColorSub_G" Pretty="G" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.3,0.6,71.5" />  
           <Attribute Name="ColorSub_B" Pretty="B" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.15,0.06,7.2" />  
           <Attribute Name="ColorSub_C" Pretty="C" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.225,0.329,78.7" />  
           <Attribute Name="ColorSub_M" Pretty="M" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.321,0.154,28.5" />  
           <Attribute Name="ColorSub_Y" Pretty="Y" ActivationGroup="ColorRGB" Feature="Color.RGB" PhysicalUnit="ColorComponent" Color="0.419,0.505,92.8" />  
           <Attribute Name="ColorMacro(n)" Pretty="Color Macro(n)" Feature="Color.RGB" />  
           <Attribute Name="ColorMacro(n)Rate" Pretty="Color Macro(n) Rate" Feature="Color.RGB" />  
           <Attribute Name="CTO" Pretty="CTO" Feature="Color.Color" PhysicalUnit="Temperature" />  
           <Attribute Name="CTC" Pretty="CTC" Feature="Color.Color" PhysicalUnit="Temperature" />  
           <Attribute Name="CTB" Pretty="CTB" Feature="Color.Color" PhysicalUnit="Temperature" />  
           <Attribute Name="Tint" Pretty="Tint" Feature="Color.Color" />  
           <Attribute Name="HSB_Hue" Pretty="H" ActivationGroup="ColorHSB" Feature="Color.HSB" PhysicalUnit="Angle" />  
           <Attribute Name="HSB_Saturation" Pretty="S" ActivationGroup="ColorHSB" Feature="Color.HSB" PhysicalUnit="Percent" />  
           <Attribute Name="HSB_Brightness" Pretty="B" ActivationGroup="ColorHSB" Feature="Color.HSB" PhysicalUnit="Percent" />  
           <Attribute Name="HSB_Quality" Pretty="Q" ActivationGroup="ColorHSB" Feature="Color.HSB" PhysicalUnit="Percent" />  
           <Attribute Name="CIE_X" Pretty="X" ActivationGroup="ColorCIE" Feature="Color.CIE" />  
           <Attribute Name="CIE_Y" Pretty="Y" ActivationGroup="ColorCIE" Feature="Color.CIE" />  
           <Attribute Name="CIE_Brightness" Pretty="B" ActivationGroup="ColorCIE" Feature="Color.CIE" PhysicalUnit="Percent" />  
           <Attribute Name="ColorRGB_Red" Pretty="R" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="ColorRGB_Green" Pretty="G" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="ColorRGB_Blue" Pretty="B" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="ColorRGB_Cyan" Pretty="C" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="ColorRGB_Magenta" Pretty="M" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="ColorRGB_Yellow" Pretty="Y" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="ColorRGB_Quality" Pretty="Q" ActivationGroup="ColorIndirect" Feature="Color.Indirect" />  
           <Attribute Name="VideoBoost_R" Feature="Color.ColorCorrection" PhysicalUnit="None" Pretty="Boost R" Color="0.64,0.33,21.3" />  
           <Attribute Name="VideoBoost_G" Pretty="Boost G" Feature="Color.ColorCorrection" PhysicalUnit="None" Color="0.3,0.6,71.5" />  
           <Attribute Name="VideoBoost_B" Pretty="Boost B" Feature="Color.ColorCorrection" PhysicalUnit="None" Color="0.15,0.06,7.2" />  
           <Attribute Name="VideoHueShift" Pretty="Hue Shift" Feature="Color.HSBC_Shift" PhysicalUnit="Angle" />  
           <Attribute Name="VideoSaturation" Pretty="S" Feature="Color.HSBC_Shift" PhysicalUnit="Percent" />  
           <Attribute Name="VideoBrightness" Pretty="B" Feature="Color.HSBC_Shift" PhysicalUnit="Percent" />  
           <Attribute Name="VideoContrast" Pretty="C" Feature="Color.HSBC_Shift" PhysicalUnit="Percent" />  
           <Attribute Name="VideoKeyColor_R" Pretty="R" Feature="Color.ColorKey" PhysicalUnit="None" Color="0.64,0.33,21.3" />  
           <Attribute Name="VideoKeyColor_G" Pretty="G" Feature="Color.ColorKey" PhysicalUnit="None" Color="0.3,0.6,71.5" />  
           <Attribute Name="VideoColorKey_B" Pretty="B" Feature="Color.ColorKey" PhysicalUnit="None" Color="0.15,0.06,7.2" />  
           <Attribute Name="VideoKeyIntensity" Pretty="Intensity" Feature="Color.ColorKey" PhysicalUnit="Percent" />  
           <Attribute Name="VideoKeyTolerance" Pretty="Tolerance" Feature="Color.ColorKey" PhysicalUnit="None" />  
           <Attribute Name="StrobeDuration" Pretty="Strobe Duration" Feature="Beam.Beam" PhysicalUnit="Time" />  
           <Attribute Name="StrobeRate" Pretty="Strobe Rate" Feature="Beam.Beam" />  
           <Attribute Name="StrobeFrequency" Pretty="Strobe Frequency" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
           <Attribute Name="StrobeModeShutter" Pretty="StrobeM Shutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModeStrobe" Pretty="StrobeM Strobe" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModePulse" Pretty="StrobeM Pulse" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModePulseOpen" Pretty="StrobeM Pulse Open" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModePulseClose" Pretty="StrobeM Pulse Close" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModeRandom" Pretty="StrobeM Random" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModeRandomPulse" Pretty="StrobeM Random Pulse" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModeRandomPulseOpen" Pretty="StrobeM Random Pulse Open" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModeRandomPulseClose" Pretty="StrobeM Random Pulse Close" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="StrobeModeEffect" Pretty="StrobeM Effect" MainAttribute="StrobeModeShutter" Feature="Beam.Beam" />  
           <Attribute Name="Shutter(n)" Pretty="Sh(n)" Feature="Beam.Beam" />  
           <Attribute Name="Shutter(n)Strobe" Pretty="Strobe(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="Duration" PhysicalUnit="Time" PhysicalFrom="0.025" PhysicalTo="0.025"/> This defines the duration of the on time of the strobe.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the strobe from the start as percentage of the total period.
           <Attribute Name="Shutter(n)StrobePulse" Pretty="Pulse(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the pulse is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the pulse from the start as percentage of the total period.
           <Attribute Name="Shutter(n)StrobePulseClose" Pretty="Pulse Close(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the ramp is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the ramp from the start as percentage of the total period.
           <Attribute Name="Shutter(n)StrobePulseOpen" Pretty="Pulse Open(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
             <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the ramp is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the ramp from the start as percentage of the total period.
           <Attribute Name="Shutter(n)StrobeRandom" Pretty="Random(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="Duration" PhysicalUnit="Time" PhysicalFrom="0.025" PhysicalTo="0.025"/> This defines the duration of the on time of the strobe.
           <Attribute Name="Shutter(n)StrobeRandomPulse" Pretty="Random Pulse(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
           <Attribute Name="Shutter(n)StrobeRandomPulseClose" Pretty="Random Pulse Close(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
           <Attribute Name="Shutter(n)StrobeRandomPulseOpen" Pretty="Random Pulse Open(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
           <Attribute Name="Shutter(n)StrobeEffect" Pretty="Effect(n)" MainAttribute="Shutter(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
           <Attribute Name="Iris" Pretty="Iris" Feature="Beam.Beam" />  
           <Attribute Name="IrisStrobe" Pretty="Strobe" MainAttribute="Iris" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="Duration" PhysicalUnit="Time" PhysicalFrom="0.3" PhysicalTo="0.3"/> This defines the duration of the on time of the iris.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the strobe from the start as percentage of the total period.
              <SubPhysicalUnit Type="MinimumOpening" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum percentage to which the iris closes.
           <Attribute Name="IrisStrobeRandom" Pretty="Random Strobe" MainAttribute="Iris" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="Duration" PhysicalUnit="Time" PhysicalFrom="0.3" PhysicalTo="0.3"/> This defines the duration of the on time of the iris.
              <SubPhysicalUnit Type="MinimumOpening" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum percentage to which the iris closes.
           <Attribute Name="IrisPulseClose" Pretty="Pulse Close" MainAttribute="Iris" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the pulse of the iris is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the pulse from the start as percentage of the total period.
              <SubPhysicalUnit Type="MinimumOpening" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum percentage to which the iris closes.
           <Attribute Name="IrisPulseOpen" Pretty="Pulse Open" MainAttribute="Iris" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the pulse of the iris is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the pulse from the start as percentage of the total period.
              <SubPhysicalUnit Type="MinimumOpening" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum percentage to which the iris closes.
           <Attribute Name="IrisRandomPulseClose" Pretty="Random Pulse Close" MainAttribute="Iris" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="MinimumOpening" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum percentage to which the iris closes.
           <Attribute Name="IrisRandomPulseOpen" Pretty="Random Pulse Open" MainAttribute="Iris" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="MinimumOpening" PhysicalUnit="Percent" PhysicalFrom="0" PhysicalTo="0"/> This defines the minimum percentage to which the iris closes.
           <Attribute Name="Frost(n)" Pretty="Frost(n)" Feature="Beam.Beam" />  
           <Attribute Name="Frost(n)PulseOpen" Pretty="Pulse Open (n)" MainAttribute="Frost(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the pulse of the frost is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the pulse from the start as percentage of the total period.
           <Attribute Name="Frost(n)PulseClose" Pretty="Pulse Close (n)" MainAttribute="Frost(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the pulse of the frost is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the pulse from the start as percentage of the total period.
           <Attribute Name="Frost(n)Ramp" Pretty="Ramp (n)" MainAttribute="Frost(n)" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
              <SubPhysicalUnit Type="DutyCycle" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the fraction of one period in which the ramp of the frost is on.
              <SubPhysicalUnit Type="TimeOffset" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the offset of the end of the ramp from the start as percentage of the total period.
           <Attribute Name="Prism(n)" Pretty="Prism(n)" ActivationGroup="Prism" Feature="Beam.Beam" />  
           <Attribute Name="Prism(n)SelectSpin" Pretty="Select Spin(n)" MainAttribute="Prism(n)" ActivationGroup="Prism" Feature="Beam.Beam" PhysicalUnit="AngularSpeed" />  
           <Attribute Name="Prism(n)Macro" Pretty="Prism(n) Macro" MainAttribute="Prism(n)" ActivationGroup="Prism" Feature="Beam.Beam" />  
           <Attribute Name="Prism(n)Pos" Pretty="Prism(n) Pos" Feature="Beam.Beam" PhysicalUnit="AngularSpeed"/>  
           <Attribute Name="Prism(n)PosRotate" Pretty="Rotate(n)" MainAttribute="Prism(n)Pos" ActivationGroup="Prism" Feature="Beam.Beam" PhysicalUnit="AngularSpeed" />  
           <Attribute Name="Effects(n)" Pretty="FX(n)" Feature="Beam.Beam" />  
           <Attribute Name="Effects(n)Rate" Pretty="FX(n) Rate" Feature="Beam.Beam" PhysicalUnit="Frequency" />  
           <Attribute Name="Effects(n)Fade" Pretty="FX(n) Fade" Feature="Beam.Beam" />  
           <Attribute Name="Effects(n)Adjust(m)" Pretty="FX(n) Adjust(m)" Feature="Beam.Beam" />  
           <Attribute Name="Effects(n)Pos" Pretty="FX(n) Pos" Feature="Beam.Beam" PhysicalUnit="Angle" />  
           <Attribute Name="Effects(n)PosRotate" Pretty="FX(n) Rotate" MainAttribute="Effects(n)Pos" Feature="Beam.Beam" PhysicalUnit="AngularSpeed" />              
           <Attribute Name="EffectsSync" Pretty="FX Sync" Feature="Beam.Beam" />  
           <Attribute Name="BeamShaper" Pretty="Beam Shaper" ActivationGroup="BeamShaper" Feature="Beam.Beam" />  
              <SubPhysicalUnit Type="RatioHorizontal" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the size of the beam compared to the original size.
              <SubPhysicalUnit Type="RatioVertical" PhysicalUnit="Percent" PhysicalFrom="100" PhysicalTo="100"/> This defines the size of the beam compared to the original size.
           <Attribute Name="BeamShaperMacro" Pretty="Beam Shaper Macro" ActivationGroup="BeamShaper" Feature="Beam.Beam" />  
           <Attribute Name="BeamShaperPos" Pretty="Beam Shaper &lt;&gt;" ActivationGroup="BeamShaper" Feature="Beam.Beam" PhysicalUnit="Angle" />  
           <Attribute Name="BeamShaperPosRotate" Pretty="Beam Shaper Rotate" ActivationGroup="BeamShaper" Feature="Beam.Beam" PhysicalUnit="AngularSpeed" />  
           <Attribute Name="Zoom" Pretty="Zoom" Feature="Focus.Focus" PhysicalUnit="Angle" />  
           <Attribute Name="ZoomModeSpot" Pretty="Zoom Spot" Feature="Focus.Focus" PhysicalUnit="Angle" />  
           <Attribute Name="ZoomModeBeam" Pretty="Zoom Beam" Feature="Focus.Focus" PhysicalUnit="Angle" />    
           <Attribute Name="DigitalZoom" Pretty="DZoom" Feature="Focus.Focus" PhysicalUnit="Angle" />        
           <Attribute Name="Focus(n)" Pretty="Focus(n)" Feature="Focus.Focus" />  
           <Attribute Name="Focus(n)Adjust" Pretty="Focus(n) Adjust" Feature="Focus.Focus" />  
           <Attribute Name="Focus(n)Distance" Pretty="Focus(n) Distance" Feature="Focus.Focus" PhysicalUnit="Length" />  
           <Attribute Name="Control(n)" Pretty="Ctrl(n)" Feature="Control.Control" />  
           <Attribute Name="DimmerMode" Pretty="Dim Mode" Feature="Control.Control" />  
           <Attribute Name="DimmerCurve" Pretty="Dim Curve" Feature="Control.Control" />  
           <Attribute Name="BlackoutMode" Pretty="Blackout Mode" Feature="Control.Control" />  
           <Attribute Name="LEDFrequency" Pretty="LED Frequency" Feature="Control.Control" PhysicalUnit="Frequency"/>  
           <Attribute Name="LEDZoneMode" Pretty="LED Zone Mode" Feature="Control.Control" />  
           <Attribute Name="PixelMode" Pretty="Pixel Mode" Feature="Control.Control" />  
           <Attribute Name="PanMode" Pretty="Pan Mode" Feature="Control.Control" />  
           <Attribute Name="TiltMode" Pretty="Tilt Mode" Feature="Control.Control" />  
           <Attribute Name="PanTiltMode" Pretty="PanTilt Mode" Feature="Control.Control" />  
           <Attribute Name="PositionModes" Pretty="Pos Modes" Feature="Control.Control" />  
           <Attribute Name="Gobo(n)WheelMode" Pretty="G(n) Mode" Feature="Control.Control" />  
           <Attribute Name="GoboWheelShortcutMode" Pretty="Gobo Shortcut Mode" Feature="Control.Control" />  
           <Attribute Name="AnimationWheel(n)Mode" Feature="Control.Control" Pretty="Anim Mode" />  
           <Attribute Name="AnimationWheelShortcutMode" Pretty="Anim Shortcut Mode" Feature="Control.Control" />  
           <Attribute Name="Color(n)Mode" Pretty="C(n) Mode" Feature="Control.Control" />  
           <Attribute Name="ColorWheelShortcutMode" Pretty="Color Wheel Shortcut Mode" Feature="Control.Control" />  
           <Attribute Name="CyanMode" Pretty="Cyan Mode" Feature="Control.Control" />  
           <Attribute Name="MagentaMode" Pretty="Magenta Mode" Feature="Control.Control" />  
           <Attribute Name="YellowMode" Pretty="Yellow Mode" Feature="Control.Control" />  
           <Attribute Name="ColorMixMode" Pretty="Color Mix Mode" Feature="Control.Control" />  
           <Attribute Name="ChromaticMode" Pretty="Chroma Mode" Feature="Control.Control" />  
           <Attribute Name="ColorCalibrationMode" Pretty="CCalib Mode" Feature="Control.Control" />  
           <Attribute Name="ColorConsistency" Pretty="Color consistency" Feature="Control.Control" />  
           <Attribute Name="ColorControl" Pretty="Color Ctrl" Feature="Control.Control" />  
           <Attribute Name="ColorModelMode" Pretty="ColorModel" Feature="Control.Control" />  
           <Attribute Name="ColorSettingsReset" Pretty="Color Ctrl Rst" Feature="Control.Control" />  
           <Attribute Name="ColorUniformity" Pretty="ColorUniform" Feature="Control.Control" />  
           <Attribute Name="CRIMode" Pretty="CRI Mode" Feature="Control.Control" />  
           <Attribute Name="CustomColor" Pretty="Custom Color" Feature="Control.Control" />  
           <Attribute Name="UVStability" Pretty="UV Stab" Feature="Control.Control" />  
           <Attribute Name="WaveLengthCorrection" Pretty="WaveLength" Feature="Control.Control" />  
           <Attribute Name="WhiteCount" Pretty="White Count" Feature="Control.Control" />  
           <Attribute Name="StrobeMode" Pretty="Strobe Mode" Feature="Control.Control" />  
           <Attribute Name="ZoomMode" Pretty="Zoom Mode" Feature="Control.Control" />  
           <Attribute Name="FocusMode" Pretty="Focus Mode" Feature="Control.Control" />  
           <Attribute Name="IrisMode" Pretty="Iris Mode" Feature="Control.Control" />  
           <Attribute Name="FanMode" Pretty="Fan Mode" Feature="Control.Control" />  
           <Attribute Name="FollowSpotMode" Pretty="FollowSpot Mode" Feature="Control.Control" />  
           <Attribute Name="BeamEffectIndexRotateMode" Pretty="Beam Effect Index Rotate Mode" Feature="Control.Control" />  
           <Attribute Name="IntensityMSpeed" Pretty="Intensity MSpeed" Feature="Control.Control" />  
           <Attribute Name="PositionMSpeed" Pretty="Pos MSpeed" Feature="Control.Control" />  
           <Attribute Name="ColorMixMSpeed" Pretty="Color Mix MSpeed" Feature="Control.Control" />  
           <Attribute Name="ColorWheelSelectMSpeed" Pretty="Color Wheel Select MSpeed" Feature="Control.Control" />  
           <Attribute Name="GoboWheel(n)MSpeed" Pretty="Gobo Wheel(n) MSpeed" Feature="Control.Control" />  
           <Attribute Name="IrisMSpeed" Pretty="Iris MSpeed" Feature="Control.Control" />  
           <Attribute Name="Prism(n)MSpeed" Pretty="Prism(n) MSpeed" Feature="Control.Control" />  
           <Attribute Name="FocusMSpeed" Pretty="Focus MSpeed" Feature="Control.Control" />  
           <Attribute Name="Frost(n)MSpeed" Pretty="Frost(n) MSpeed" Feature="Control.Control" />  
           <Attribute Name="ZoomMSpeed" Pretty="Zoom MSpeed" Feature="Control.Control" />  
           <Attribute Name="FrameMSpeed" Pretty="Frame MSpeed" Feature="Control.Control" />  
           <Attribute Name="GlobalMSpeed" Pretty="Global MSpeed" Feature="Control.Control" />  
           <Attribute Name="ReflectorAdjust" Pretty="Reflector Adj" Feature="Control.Control" /> />  
           <Attribute Name="FixtureGlobalReset" Pretty="Fixture Global Reset" Feature="Control.Control" />  
           <Attribute Name="DimmerReset" Pretty="Dimmer Reset" Feature="Control.Control" />  
           <Attribute Name="ShutterReset" Pretty="Shutter Reset" Feature="Control.Control" />  
           <Attribute Name="BeamReset" Pretty="Beam Reset" Feature="Control.Control" />  
           <Attribute Name="ColorMixReset" Pretty="Color Mix Reset" Feature="Control.Control" />  
           <Attribute Name="ColorWheelReset" Pretty="Color Wheel Reset" Feature="Control.Control" />  
           <Attribute Name="FocusReset" Pretty="Focus Reset" Feature="Control.Control" />  
           <Attribute Name="FrameReset" Pretty="Frame Reset" Feature="Control.Control" />  
           <Attribute Name="GoboWheelReset" Pretty="G Reset" Feature="Control.Control" />  
           <Attribute Name="IntensityReset" Pretty="Intensity Reset" Feature="Control.Control" />  
           <Attribute Name="IrisReset" Pretty="Iris Reset" Feature="Control.Control" />  
           <Attribute Name="PositionReset" Pretty="Pos Reset" Feature="Control.Control" />  
           <Attribute Name="PanReset" Pretty="Pan Reset" Feature="Control.Control" />  
           <Attribute Name="TiltReset" Pretty="Tilt Reset" Feature="Control.Control" />  
           <Attribute Name="ZoomReset" Pretty="Zoom Reset" Feature="Control.Control" />  
           <Attribute Name="CTBReset" Pretty="CTB Reset" Feature="Control.Control" />  
           <Attribute Name="CTOReset" Pretty="CTO Reset" Feature="Control.Control" />  
           <Attribute Name="CTCReset" Pretty="CTC Reset" Feature="Control.Control" />  
           <Attribute Name="AnimationSystemReset" Pretty="Anim Sytem Reset" Feature="Control.Control" />  
           <Attribute Name="FixtureCalibrationReset" Pretty="Fixture Calibration Reset" Feature="Control.Control" />  
           <Attribute Name="Function" Pretty="Function" Feature="Control.Control" />  
           <Attribute Name="LampControl" Pretty="Lamp Ctrl" Feature="Control.Control" />  
           <Attribute Name="DisplayIntensity" Pretty="Display Int" Feature="Control.Control" />  
           <Attribute Name="DMXInput" Pretty="DMX Input" Feature="Control.Control" />  
           <Attribute Name="NoFeature" Pretty="NoFeature" Feature="Control.Control" />  
           <Attribute Name="Dummy" Pretty="Dummy" Feature="Control.Control" />  
           <Attribute Name="Blower(n)" Pretty="Blower(n)" Feature="Control.Control" />  
           <Attribute Name="Fan(n)" Pretty="Fan(n)" Feature="Control.Control" />  
           <Attribute Name="Fog(n)" Pretty="Fog(n)" Feature="Control.Control" />  
           <Attribute Name="Haze(n)" Pretty="Haze(n)" Feature="Control.Control" />  
           <Attribute Name="LampPowerMode" Pretty="Lamp Power Mode" Feature="Control.Control" />  
           <Attribute Name="Fans" Pretty="Fans" Feature="Control.Control" />  
           <Attribute Name="Blade(n)A" Pretty="Blade(n)A" ActivationGroup="Shaper" Feature="Shapers.Shapers" />  
           <Attribute Name="Blade(n)B" Pretty="Blade(n)B" ActivationGroup="Shaper" Feature="Shapers.Shapers" />  
           <Attribute Name="Blade(n)Rot" Pretty="Blade(n) Rot" ActivationGroup="Shaper" Feature="Shapers.Shapers" PhysicalUnit="Angle" />  
           <Attribute Name="ShaperRot" Pretty="Shaper Rot" ActivationGroup="Shaper" Feature="Shapers.Shapers" PhysicalUnit="Angle" />  
           <Attribute Name="ShaperMacros" Pretty="Shaper Macros" Feature="Shapers.Shapers" />  
           <Attribute Name="ShaperMacrosSpeed" Pretty="Shaper Macros Speed" Feature="Shapers.Shapers" />  
           <Attribute Name="BladeSoft(n)A Pretty="BladeS(n)A" "Feature="Shapers.Shapers" PhysicalUnit="None" />  
           <Attribute Name="BladeSoft(n)B" Pretty="BladeS(n)B" Feature="Shapers.Shapers" PhysicalUnit="None" />  
           <Attribute Name="KeyStone(n)A" Pretty="KS(n)A" Feature="Shapers.Shapers" PhysicalUnit="None" />  
           <Attribute Name="KeyStone(n)B" Pretty="KS(n)B" Feature="Shapers.Shapers" PhysicalUnit="None" />  
           <Attribute Name="Video" Pretty="Video" Feature="Video.Video" />  
           <Attribute Name="VideoEffect(n)Type" Pretty="Video Effect(n) Type" Feature="Video.Video" />  
           <Attribute Name="VideoEffect(n)Parameter(m)" Pretty="Video Effect(n) Parameter(m)" Feature="Video.Video" />  
           <Attribute Name="VideoCamera(n)" Pretty="Video Camera(n)" Feature="Video.Video" />  
           <Attribute Name="FieldOfView" Pretty="FOV" Feature="Video.Video" PhysicalUnit="Angle" />  
           <Attribute Name="InputSource" Pretty="ISrc" Feature="Video.Video" PhysicalUnit="None" />  
           <Attribute Name="VideoBlendMode" Pretty="BlendMode" Feature="Video.Video" PhysicalUnit="None" />  
           <Attribute Name="VideoSoundVolume(n)" Pretty="Volume(n)" Feature="Video.Video" PhysicalUnit="Percent" />  
       </Attributes>  
   </AttributeDefinitions>

```

Example for enumeration:

```
   <Attribute Name="Gobo1" Pretty="G1" ActivationGroup="Gobo1" Feature="Gobo.Gobo" />  
   <Attribute Name="Gobo2" Pretty="G2" ActivationGroup="Gobo2" Feature="Gobo.Gobo" />
```


## Annex C (informative) Name character table

Names are UTF-8 literals. In the first 128 characters only use
characters listed below. All characters above `127` are available.

<div id="table-c1">

##### Table C1. *UTF-8 table*

| Unicode code point | Character | UTF-8 (dec.) | Name                   |
| ------------------ | --------- | ------------ | ---------------------- |
| U+0020             |           | 32           | SPACE                  |
| U+0022             | `"`       | 34           | QUOTATION MARK         |
| U+0023             | `#`       | 35           | NUMBER SIGN            |
| U+0025             | %         | 37           | PERCENT SIGN           |
| U+0027             | '         | 39           | APOSTROPHE             |
| U+0028             | (         | 40           | LEFT PARENTHESIS       |
| U+0029             | )         | 41           | RIGHT PARENTHESIS      |
| U+002A             | `*`       | 42           | ASTERISK               |
| U+002B             | \+        | 43           | PLUS SIGN              |
| U+002D             | \-        | 45           | HYPHEN-MINUS           |
| U+002F             | /         | 47           | SOLIDUS                |
| U+0030             | 0         | 48           | DIGIT ZERO             |
| U+0031             | 1         | 49           | DIGIT ONE              |
| U+0032             | 2         | 50           | DIGIT TWO              |
| U+0033             | 3         | 51           | DIGIT THREE            |
| U+0034             | 4         | 52           | DIGIT FOUR             |
| U+0035             | 5         | 53           | DIGIT FIVE             |
| U+0036             | 6         | 54           | DIGIT SIX              |
| U+0037             | 7         | 55           | DIGIT SEVEN            |
| U+0038             | 8         | 56           | DIGIT EIGHT            |
| U+0039             | 9         | 57           | DIGIT NINE             |
| U+003A             | `:`       | 58           | COLON                  |
| U+003B             | `;`       | 59           | SEMICOLON              |
| U+003C             | \<        | 60           | LESS-THAN SIGN         |
| U+003D             | \=        | 61           | EQUALS SIGN            |
| U+003E             | \>        | 62           | GREATER-THAN SIGN      |
| U+0040             | @         | 64           | COMMERCIAL AT          |
| U+0041             | A         | 65           | LATIN CAPITAL LETTER A |
| U+0042             | B         | 66           | LATIN CAPITAL LETTER B |
| U+0043             | C         | 67           | LATIN CAPITAL LETTER C |
| U+0044             | D         | 68           | LATIN CAPITAL LETTER D |
| U+0045             | E         | 69           | LATIN CAPITAL LETTER E |
| U+0046             | F         | 70           | LATIN CAPITAL LETTER F |
| U+0047             | G         | 71           | LATIN CAPITAL LETTER G |
| U+0048             | H         | 72           | LATIN CAPITAL LETTER H |
| U+0049             | I         | 73           | LATIN CAPITAL LETTER I |
| U+004A             | J         | 74           | LATIN CAPITAL LETTER J |
| U+004B             | K         | 75           | LATIN CAPITAL LETTER K |
| U+004C             | L         | 76           | LATIN CAPITAL LETTER L |
| U+004D             | M         | 77           | LATIN CAPITAL LETTER M |
| U+004E             | N         | 78           | LATIN CAPITAL LETTER N |
| U+004F             | O         | 79           | LATIN CAPITAL LETTER O |
| U+0050             | P         | 80           | LATIN CAPITAL LETTER P |
| U+0051             | Q         | 81           | LATIN CAPITAL LETTER Q |
| U+0052             | R         | 82           | LATIN CAPITAL LETTER R |
| U+0053             | S         | 83           | LATIN CAPITAL LETTER S |
| U+0054             | T         | 84           | LATIN CAPITAL LETTER T |
| U+0055             | U         | 85           | LATIN CAPITAL LETTER U |
| U+0056             | V         | 86           | LATIN CAPITAL LETTER V |
| U+0057             | W         | 87           | LATIN CAPITAL LETTER W |
| U+0058             | X         | 88           | LATIN CAPITAL LETTER X |
| U+0059             | Y         | 89           | LATIN CAPITAL LETTER Y |
| U+005A             | Z         | 90           | LATIN CAPITAL LETTER Z |
| U+005F             | \_        | 95           | LOW LINE               |
| U+0060             | \`        | 96           | GRAVE ACCENT           |
| U+0061             | a         | 97           | LATIN SMALL LETTER A   |
| U+0062             | b         | 98           | LATIN SMALL LETTER B   |
| U+0063             | c         | 99           | LATIN SMALL LETTER C   |
| U+0064             | d         | 100          | LATIN SMALL LETTER D   |
| U+0065             | e         | 101          | LATIN SMALL LETTER E   |
| U+0066             | f         | 102          | LATIN SMALL LETTER F   |
| U+0067             | g         | 103          | LATIN SMALL LETTER G   |
| U+0068             | h         | 104          | LATIN SMALL LETTER H   |
| U+0069             | i         | 105          | LATIN SMALL LETTER I   |
| U+006A             | j         | 106          | LATIN SMALL LETTER J   |
| U+006B             | k         | 107          | LATIN SMALL LETTER K   |
| U+006C             | l         | 108          | LATIN SMALL LETTER L   |
| U+006D             | m         | 109          | LATIN SMALL LETTER M   |
| U+006E             | n         | 110          | LATIN SMALL LETTER N   |
| U+006F             | o         | 111          | LATIN SMALL LETTER O   |
| U+0070             | p         | 112          | LATIN SMALL LETTER P   |
| U+0071             | q         | 113          | LATIN SMALL LETTER Q   |
| U+0072             | r         | 114          | LATIN SMALL LETTER R   |
| U+0073             | s         | 115          | LATIN SMALL LETTER S   |
| U+0074             | t         | 116          | LATIN SMALL LETTER T   |
| U+0075             | u         | 117          | LATIN SMALL LETTER U   |
| U+0076             | v         | 118          | LATIN SMALL LETTER V   |
| U+0077             | w         | 119          | LATIN SMALL LETTER W   |
| U+0078             | x         | 120          | LATIN SMALL LETTER X   |
| U+0079             | y         | 121          | LATIN SMALL LETTER Y   |
| U+007A             | z         | 122          | LATIN SMALL LETTER Z   |
|                    |           |              |                        |


</div>


## Annex D (informative) Predefined Connector Types

Table D.1 table shows the predefined connector types.

<div id="table-d1">

#### Table D1. *Predefined Connector Types*

| Type             | Description                |
| ---------------- | -------------------------- |
| BNC              | BNC connector              |
| TBLK             | Tag block                  |
| TAG              | Solder tag                 |
| KRN              | Krone block                |
| STJ              | Stereo jack                |
| MSTJ             | Mini stereo jack           |
| RCA              | Phono connector            |
| SCART            | SCART connector            |
| SVIDEO           | 4-pin mini-DIN             |
| MDIN4            | 4-pin mini-DIN             |
| MDIN5            | 5-pin mini-DIN             |
| MDIN6            | 6-pin mini-DIN             |
| XLR3             | 3-pin XLR                  |
| XLR4             | 4-pin XLR                  |
| XLR5             | 5-pin XLR                  |
| RJ45             | 10/100 BaseT ethernet type |
| RJ11             | Telephone type             |
| DB9              | 9-pin D-type               |
| DB15             | 15-pin D-type              |
| DB25             | 25-pin D-type              |
| DB37             | 37-pin D-type              |
| DB50             | 50-pin D-type              |
| HD15             | 15-pin D-type hi-density   |
| HD25             | 25-pin D-type hi-density   |
| DIN3             | 3-pin DIN                  |
| DIN5             | 5-pin DIN                  |
| EDAC20           | EDAC 20-pin                |
| EDAC38           | EDAC 38-pin                |
| EDAC56           | EDAC 56-pin                |
| EDAC90           | EDAC 90-pin                |
| EDAC120          | EDAC 120-pin               |
| DL96             | DL 96-pin                  |
| SCSI68           | SCSI connector 68-pin      |
| IEE488           | IEE488 connector 36-pin    |
| CENT50           | Centronics 50-pin          |
| CENT36           | Centronics 36-pin          |
| CENT24           | Centronics 24-pin          |
| DisplayPort      | DisplayPort connector      |
| DVI              | DVI connector              |
| HDMI             | HDMI connector             |
| PS2              | PS2 connector              |
| TL-ST            | TosLink connector          |
| LCDUP            | Fiber optic LC DUPLEX-type |
| SCDUP            | Fiber optic SC DUPLEX-type |
| SC               | Fiber optic SC-type        |
| ST               | Fiber optic ST-type        |
| NL4              | Speakon                    |
| CACOM            | 8-pin LS conn              |
| USB              | USB connector              |
| N\_CON           | N connector                |
| F\_CON           | F connector                |
| IEC 60320-C7/C8  | Eurostecker                |
| CEE 7/7          | Schutzkontakt              |
| IEC 60320-C13/14 | IEC 60320                  |
| Edison           | Edison                     |
| Wieland          | Wieland                    |
| 16A-CEE-2P       | 16A-Blue                   |
| 16A-CEE-2P-110   | 16A-Yellow                 |
| 16A-CEE          | 16A-CEE                    |
| 32A-CEE          | 32A-CEE                    |
| 32A-CEE-2P       | 32A-Blue                   |
| 32A-CEE-2P-110   | 32A-Yellow                 |
| 63A-CEE          | 63A-CEE                    |
| 125A-CEE         | 125A-CEE                   |
| Powerlock        | Powerlock                  |
| Powerlock 120A   | Powerlock 120A             |
| Powerlock 400A   | Powerlock 400A             |
| Powerlock 660A   | Powerlock 660A             |
| Powerlock 800A   | Powerlock 800A             |
| Camlock          | Camlock                    |
| NAC3FCA          | Powercon Blue              |
| NAC3FCB          | Powercon Grey              |
| PowerconTRUE1    | Powercon TRUE1             |
| powerCONTRUE1TOP | powerCON TRUE1 TOP         |
| Socapex-16       | Socapex-16                 |
| Socapex-7        | Socapex-7                  |
| Socapex-9        | Socapex-9                  |
| HAN-16           | HAN-16                     |
| HAN-4            | HAN-4                      |
| L6-20            | L6-20                      |
| L15-30           | L15-30                     |
| Stagepin         | Stagepin                   |
| HUBBELL-6-4      | HUBBELL 6-4                |
| DIN 56905        | Eberl                      |


</div>

## Annex E (normative) Wheel Slot Image Definition

Definition of images used in wheel slots to visualize gobos, animation
wheels or color wheels.

Gobo images shall be in PNG format with an alpha channel. Indexed,
Greyscale and Alpha, 8bit RGB and Alpha, or 16bit RGB and Alpha are
accepted pixel formats.

![Gobo Labeled](media/Gobo_example_labeled.png)

A gobo image is comprised of a transparent Background (1) and the image
itself on top. The Background shall be fully transparent and should be
considered to be the equivalent of a gobo holder. The Image region shall
be fully opaque aside from anti-aliasing, and shall be as large as
possible.

Note 1: This allows a data consumer to determine the precise pixel extents of
the Image and place the provided PNG over an arbitrary GUI background without
additional processing.

The Background region, the equivalent of gobo holder, is defined by full
transparency (Alpha 0). In the Image region, Pure Black (RGB 0,0,0) is
opaque (2), and Pure White (RGB 255,255,255) is transparent (letter A).

Colored gobos (3) shall use an RGB approximation. The RGB approximation
shall be calculated on the basis of Pure White being the CCT of the
fixture light source and the ICC color profile embedded within the PNG.
(See ISO 15076-1:2010) The default shall be sRGB.

## Annex F (normative) SubPhysicalUnit precisions

### Pulse

Some attributes are named "[EFFECT]Pulse" (for example Shutter(n)StrobePulse). They should behave like a triangular function. Here is how they are defined:
- The main PhysicalUnit is a **frequency** which allows to modify the duration of the period T.
- t = 0s corresponds to the activation of the channel function.
- The **TimeOffset** SubPhysicalUnit is the offset of the end of the pulse from 0, the activation of the channel function, as a percentage of the period T.
- The **DutyCycle** SubPhysicalUnit corresponds to the fraction of one period in which the value is not 0.

![Graph of a Pulse-like attribute and their SubPhysicalUnits](media/Pulse.png "Pulse-like attribute and their SubPhysicalUnits")

*Figure 5. Pulse-like attribute and their SubPhysicalUnits*

If there aren't any value given by the user, the TimeOffset and DutyCycle SubPhysicalUnits are defaulted to 100%:

![Graph of a default Pulse-like attribute and their SubPhysicalUnits](media/Pulse\_Default.png "default Pulse-like attribute and their SubPhysicalUnits")

*Figure 6. Default Pulse-like attribute and their SubPhysicalUnits*

### PulseClose

Some attributes are named "[EFFECT]PulseClose" (for example IrisPulseClose). They should behave like a decreasing ramp. Here is how they are defined:
- The main PhysicalUnit is a **frequency** which allows to modify the duration of the period T.
- t = 0s corresponds to the activation of the channel function.
- The **TimeOffset** SubPhysicalUnit is the offset of the end of the pulse from 0, the activation of the channel function, as a percentage of the period T.
- The **DutyCycle** SubPhysicalUnit corresponds to the fraction of one period in which the value is not 0.

![Graph of a PulseClose-like attribute and their SubPhysicalUnits](media/PulseClose.png "PulseClose-like attribute and their SubPhysicalUnits")

*Figure 7. PulseClose-like attribute and their SubPhysicalUnits*

If there aren't any value given by the user, the TimeOffset and DutyCycle SubPhysicalUnits are defaulted to 100%:

![Graph of a default PulseClose-like attribute and their SubPhysicalUnits](media/PulseClose\_Default.png "default PulseClose-like attribute and their SubPhysicalUnits")

*Figure 8. Default PulseClose-like attribute and their SubPhysicalUnits*

### PulseOpen

Some attributes are named "[EFFECT]PulseOpen" (for example Frost(n)PulseOpen). They should behave like an increasing ramp. Here is how they are defined:
- The main PhysicalUnit is a **frequency** which allows to modify the duration of the period T.
- t = 0s corresponds to the activation of the channel function.
- The **TimeOffset** SubPhysicalUnit is the offset of the end of the pulse from 0, the activation of the channel function, as a percentage of the period T.
- The **DutyCycle** SubPhysicalUnit corresponds to the fraction of one period in which the value is not 0.

![Graph of a PulseOpen-like attribute and their SubPhysicalUnits](media/PulseOpen.png "PulseOpen-like attribute and their SubPhysicalUnits")

*Figure 9. PulseOpen-like attribute and their SubPhysicalUnits*

If there aren't any value given by the user, the TimeOffset and DutyCycle SubPhysicalUnits are defaulted to 100%:

![Graph of a default PulseOpen-like attribute and their SubPhysicalUnits](media/PulseOpen\_Default.png "default PulseOpen-like attribute and their SubPhysicalUnits")

*Figure 10. Default PulseOpen-like attribute and their SubPhysicalUnits*

# Revision History

This section lists all the changes that are made to GDTF.

## Version 1.1

  - Added connectors collect to the physical description; \#135
  - Added media server attributes. Edited existing media server
    attributes; \#134
  - Added geometry types MediaServerLayer, MediaServerCamera,
    MediaServerMaster, Display; \#133
  - Added detailed definition how a gobo image is interpreted \#107
  - Added more detailed definition of Geometry Type "Beam" \#132
  - Changed value type of Channel Function XML attribute "Name" \#111
  - Added XML property "CanHaveChildren" to fixture type node. \#144
  - Added LegHeight to the properties collect \#147
  - Added description about the origin of a fixture \#147
  - Added XML attribute "InitialFunction" to DMX Channel, Moved Default
    from DMX Channel to Channel Function \#153
  - Updated XML attribute "Color" of Emitter, Filter and Wheel Slot
    \#112
  - Updated XML attribute "LuminousFlux" of Geometry type Beam \#158

Note: The default meshes for Base, Conventional and Scanner were updated. The
origin of the meshes is now the mounting plate.

## Version 1.0

  - Geometry Type Axis: Removed XML attributes "From", "To" and "Speed";
    \#2
  - Changed enumeration of attribute names; \#33
  - Node Revision: added XML attribute "UserID"; \#17
  - Revisions are optional now. Every time a GDTF file is uploaded to
    GDTF-Share.com a revision with the actual time and UserID is created
    by the GDTF-Share; \#17
  - Geometry Type Geometry Reference: XML attribute "Model" added. \#24
  - All Geometry Nodes: XML attribute "Model" now uses value type
    "Name". \#77
  - Node Model: Changed description of XML attribute "File"; \#79
  - ChannelFunction: removed XML attribute DMXInvert; \#31
  - DMX Mode Children: Removed RDM Personality; \#79
  - Node DMXPersonality: Changed Value type of XML attribute DMXMode;
    \#79
  - Physical Description: Updated definition of emitters; \#34
  - Physical Description: Added definition of filters; \#34
  - Channel Function: Added XML attribute "Filter"; \#34
  - Wheel Slot: Added XML attribute "Filter"; \#34
  - Physical Description: Added definition of color space for indirect
    color mixing; \#34
  - Updated suggested names for Geometry Type "Geometry" and "Axis";
    \#19
  - Updated appendix A and B due to rework and addition of attributes;
    \#35
  - DMXChannel: Removed XML attributes "Coarse", "Fine", "Ultra" and
    "Uber" and added XML attribute "Offset" instead.
  - Fixture Type Node: Added XML attribute "LongName"; \#78
  - Fixture Type Node: Changed description of XML attribute "ShortName";
    \#78
  - Fixture Type Node: Changed description of XML attribute
    "Manufacturer"; \#78

## Version 0.9

  - Geometries are now always referenced by Name and not by Node Link;
    \#23
  - Fixed glitch in spec regarding ZIP-archive; \#8
  - Updated example for Data Version; \#4
  - Updated description of Model Node - Replaced wheel slot by model;
    \#5
  - Geometry type Beam: Changed name of XML attribute
    "LuminousIntensity" to "LuminousFlux";
  - Changed definition of value type "Date" corresponding to UTC;
  - Maximum count of vertices is now defined per device and not per
    model;
  - Removed optional checksum per file; \#7
  - Fixture type node: Added .svg as additional resource type for
    thumbnail; \#25
  - Changed maximum resolution of png files for thumbnail and wheel slot
    to 1024x1024; \#18
  - Updated format of value type "GUID" corresponding to RFC 4122; \#15
  - Defined folder structure for resource files; \#11 \#28
  - Links to all resource files do not have a file extension any longer;
  - Added first iteration of media server attributes; \#41
  - Added table for allowable UTF-8 chars for Names; \#64
  - Node ChannelSet: Changed description of XML attribute
    "WheelSlotIndex"; \#43
  - Defined more clearly how the wheel slot index is created; \#43
  - Updated RDM Section; \#69
  - Node DMXChannel: The XML attribute "Frequency" was removed; \#31
  - Node ChannelFunction: The XML attribute "EncoderInvert" was removed;
    \#31
  - Moved XML attributes "MibFade" and "DMXChangeTimeLimit" from the
    DMXChannel to the LogicalChannel; \#31

## Version 0.88

  - Node ChannelSet: Removed defaults in XML attributes PhysicalFrom and
    PhysicalTo;
  - Node ChannelSet: Changed description of XML attributes Name and
    DMXFrom;
  - Node DMXChannel: Changed description of XML attribute Default;
  - Node Relation: Renamed XML attribute Slave to Follower;
  - Node Relation: Removed XML attributes DMXFrom and DMXTo;
  - Node Relation: Removed relation type “Mode”;
  - Node ChannelFunction: Added XML attributes ModeMaster, ModeFrom and
    ModeTo;
  - Added attributes CIE\_X, CIE\_Y and CIE\_Brightness;
  - Reworked and added further attributes for gobo wheel, color wheel,
    color mixing and prism.
  - Updated Appendix A and Appendix B as per changes made in attributes.

## Version 0.87

  - Changing format of type Matrix and Rotation;
  - New XML attribute of DMX Channel - Uber;
  - Attribute has no more Special XML attribute;
  - Pigtail position should not be specified in 2D or 3D files anymore.
    Instead of it should be created a general geometry and linked to a
    model with primitive type “Pigtail”;
  - Measurement point does not have DMX and Color XML properties
    anymore;
  - Added specification to 3D mesh;
  - Added new part Color rendering index collect;
  - Added new part Supported protocol collect and moved RDM section to
    this part;
  - GDTF file should have extension “.gdtf”;
  - Subattributes are no more part of GDTF. Instead Attributes get a new
    XML attribute “MainAttribute”;
  - Macro Collect moved into DMX Mode and is defined;
  - Channel Set has no more XML attribute Real Fade;
  - Defined measurement resolution for Emitters;
  - Logical Channel and DMX Channel has an automatically generated name,
    which cannot be specified in XML.

## Version 0.86

  - Changing XML tag for Emitter collect;
  - XML node “Master” moved from Channel Function to Logical Channel;
  - New predefined values for primitive type of model;
  - Lamp geometry type is renamed to Beam;
  - New name suggestion of General Geometry;
  - Attribute has new XML attribute – “Special”;
  - Default SubAttribute for Channel Function is NoFeature;
  - Fixture Type has new XML attribute “RefFT”;
  - RealFade XML attribute has type float;
  - Renaming of XML Attribute MibFadeFrame to MibFade, type float;
  - Changing type of XML attribute DMXChangeTimeLimit to float;
  - Beam has new XML attribute FieldAngle;
  - Wheel no longer has the XML attribute “SubAttribute”;
  - Changing XML tags of EmitterCollect to Emitters; DMXProfileCollect
    to DMXProfiles;
  - New Type “DMXValue”, used to specify DMX values like Default or
    DMXFrom;
  - Adjusted names of predefined ActivationGroups, Attributes and
    Subattributes;
  - Removed predefined Attributes “MasterIntensity”, REDALL, GREENALL,
    BLUEALL, AMBERALL, WHITEALL;
  - Removed ActivationGroup “ColorRGBALL”.

## Version 0.85

  - Internal XML file has a static name “description.xml”.
  - DMX mode collect should contain all modes and all firmware
    revisions.

