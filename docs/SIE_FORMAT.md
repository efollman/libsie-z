# The SIE Format

> **Original authors:** Brian Downing & Chris LaReau, HBM, Inc. (December 2009)
>
> This document is ported from the original LaTeX source (`sie-format.tex`).
> The original C library is available at
> [github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference)
> or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).
> The SIE file format itself is unchanged — this document describes the
> on-disk/wire format that both the C and Zig implementations read and write.
> Zig-specific notes are called out in clearly marked blocks.

---

## Abstract

SIE is a data transmission and storage format for engineering data.  SIE is
designed to be flexible, self-describing, streamable, and robust, overcoming
some of the limitations of existing engineering data formats.  This is
accomplished with a simple block structure and an XML description of how to
read both the block structure and contained binary data.  It is also very
simple to write, as some data loggers have very limited processor and memory
capacity.  SIE has been successfully implemented on SoMat's eDAQ line of data
acquisition systems, and a library for reading SIE files has been implemented
in portable C (libsie) and in pure Zig (libsie-zig).

---

## 1. Introduction

### 1.1 Requirements

Many of the requirements of a data format are dictated by the end user of the
format — those who read it.  At the time of writing, a 32 gigabyte
CompactFlash card is available for around US$80, and the trend of increasing
capacity and decreasing price continues.  Putting 16 or more gigabytes of
storage into a small data acquisition system is increasingly common.

For situations involving long-term testing, it is desired to be able to read
some or all of the data while testing is in progress.  In other words, data
stored in a file should be readable while the file is still being written to.

As more information enters the digital world, it is becoming more realistic to
want many kinds of information associated with engineering data.  A data format
should be flexible enough to store arbitrary text, pictures, audio, video, and
other information not yet conceived.

Finally, and most importantly, nobody wants to lose data they've collected.
Testing is very expensive, and hard or impossible to repeat.  Any data storage
format needs to be as robust as possible to reduce the frequency of problems,
and to make recovering data as easy as possible when catastrophe occurs.

Additionally, some requirements are driven by implementers of hardware and
software that writes engineering data.  When collecting data, it usually comes
packed in some specific binary format.  An ideal data format would offer
flexible binary storage options so that the incoming data could be directly
written out, or at the very least with minimum changes.

Also, a good format should allow the details of the binary data stored to be
changed, without changing the engineering values represented, and without
requiring modifications to the reading software.

Finally, the format should be very easy to write.  Ideally, it should be
feasible to write the format on microcontrollers with only kilobytes of RAM.

### 1.2 Problems with existing formats

Almost all existing engineering data formats have certain undesirable traits.

Most existing formats are not *streamable* — they require random access to
write and read, usually because they have forward-pointing intra-file
references.  This limits the ability to send data in real time across a
network.  Also, the presence of forward pointers makes a file format more
complicated and error-prone.  If a file is corrupted and key linking
information is destroyed, it can be difficult or impossible to recover
otherwise unaffected data.

In some cases, files may not be in a consistent, readable state until writing
has stopped, either due to missing forward-pointing offsets or other design
issues.

In addition, many engineering data formats use 32-bit offsets internally,
limiting files to no more than 2³¹−1 or 2³² bytes.

Finally, many existing data formats have limited metadata representations.

### 1.3 Design goals

- **Flexible** — It should be possible to encode a wide variety of internal
  data formats without needing to extend the SIE specification.
- **Conceptually simple and self-describing** — It should be possible to look
  at an SIE file and write code to extract data from it.  Explicit data model,
  no implicit knowledge.
- **No arbitrary limits** — File size, data formats, metadata size/format/
  language.
- **Streamable** — No forward data pointers.  Append-only file writing.
- **Safe** — Transmission or storage error detection.  Maximal recoverability
  in case of corruption.
- **Efficient** — Minimal overhead as a percentage of data size.  Processing
  should allow storing data in native transducer format.

---

## 2. Design

At its core, SIE is a block-structured container for data.  It defines an XML
schema for describing both the structure of the data and their associated
metadata.  It also defines an optional index format to speed up file access.

### 2.1 Data model

SIE presents a flexible, unified data model to the end user.  Data are
presented as a table of rows and columns.  For example, a time series channel
could be represented as:

| | **Dimension 0** | **Dimension 1** |
|---|---|---|
| **Row 0** | 0.0 | 0.42 |
| **Row 1** | 0.1 | −0.20 |
| **Row 2** | 0.2 | 0.13 |
| **Row 3** | 0.3 | 0.06 |
| **Row 4** | 0.4 | −0.23 |
| … | … | … |

A dimension (column) may contain either numerical values or "raw" binary data.

The metadata for this example would contain additional information, such as the
fact that dimension 0 is *time* and the applicable unit is *seconds*, while
dimension 1 is the actual data measurement, for which the unit is *millivolts*.

Ultimately, all SIE data are presented in this basic form.  It is notable that,
if given an SIE file with completely unknown data, all of the data values will
be immediately visible even if the details of the intended representation are
unknown.

Metadata are represented as *tags*, which are pairings of a textual name to
arbitrary binary data.  Tags can exist at any level of the SIE hierarchy.  The
metadata for the example above could be represented as:

```xml
<ch id="0" name="example">
  <dim index="0">
    <tag id="core:label">time</tag>
    <tag id="core:units">seconds</tag>
  </dim>
  <dim index="1">
    <tag id="core:label">measurement</tag>
    <tag id="core:units">millivolts</tag>
  </dim>
</ch>
```

To help standardize and document the many ways data and metadata can be
represented on this basic model, SIE has *schemas*, which are documented sets
of data and metadata representation standards.  Schemas are identified by a
namespacing system — for example, all tags beginning with `core:` belong to the
`core` schema.  The `sie` schema is reserved for items defined in the SIE
format proper.  Two other schemas exist: `core`, which describes metadata
applicable to most engineering data, and `somat`, which describes metadata and
data representations specific to SoMat's line of data acquisition systems.  See
[CORE_SCHEMA.md](CORE_SCHEMA.md) and [SOMAT_SCHEMA.md](SOMAT_SCHEMA.md) for
details.

Note that data can be *stored* in SIE in almost any conceivable format.  Most
of the SIE format is in fact a mechanism to convert arbitrary binary data in a
block-structured container into the unified data model described above.

### 2.2 Block structure

SIE is output as a stream of bytes, composed of a series of data blocks in
sequence without intervening padding.  A block is composed of several parts:

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 4 | *size* | Total size of the block in bytes. |
| 4 | 4 | *group* | Group identifier for this block. |
| 8 | 4 | *sync word* | Constant `0x51EDA7A0`. |
| 12 | *n* | *payload* | *n* bytes of data. |
| *n* + 12 | 4 | *checksum* | CRC-32 of bytes 0 through *n* + 11 inclusive. |
| *n* + 16 | 4 | *size 2* | Same as *size*. |

*(All sizes and offsets in bytes.  All multi-byte fields are 32-bit unsigned
integers in network/big-endian byte order.)*

The **size** is the total size of the block in bytes, including both size
fields.  Having the size on both sides allows SIE data to be traversed both
forwards and backwards, and provides one level of consistency checking — if the
sizes don't agree, something is wrong.

The **group** identifies what data are contained in the payload.  Groups 0
and 1 are always the XML metadata and index blocks respectively.  The purpose
of all other groups is defined in the XML metadata.

The **sync word** is always `0x51EDA7A0` (looks vaguely like "SIEDATA0").  It
is used to find the beginning of a block (or "resync") in cases such as
damaged files or joining a broadcast stream in the middle.  As it has some high
bits set, if an SIE file is transmitted over a medium that is not 8-bit clean,
the sync word will be damaged, quickly indicating corruption.

The **payload** is the actual data contained in the block, interpreted based
on the group.  The length of the payload is always *size* − 20.

The **checksum** is a CRC-32 checksum of all bytes from the first byte of the
first *size* through the last byte of the *payload*.  The checksum is optional
(some devices may not have the processor capacity to compute it), but if not
present the checksum field must be set to zero.  When reading a block, a zero
checksum is always valid.

A block with a payload size of zero indicates that no more blocks of that group
will occur.  This is intended for end-of-data signaling in real-time streaming
scenarios.

> **Zig implementation note:** The `Block` struct in `src/block.zig` directly
> models this layout.  `Block.parseFromData()` reads the above format from a
> byte slice.  `Block.writeTo()` serializes it.  `Block.validateChecksum()`
> verifies the CRC-32.  The sync word constant is defined as
> `[4]u8{ 0x51, 0xED, 0xA7, 0xA0 }`.

### 2.3 Predefined groups

There are only two groups defined in the core SIE standard: group 0 blocks
contain the SIE XML metadata, and group 1 blocks are index blocks.

#### 2.3.1 XML metadata (group 0)

The SIE XML metadata defines how to interpret the SIE data.  It contains:

- **Tags** — generalized metadata relating an arbitrary text key to a
  completely arbitrary value.
- **Channels** — groupings of engineering data (composed of multiple
  *dimensions*) with their associated metadata.
- **Tests** — groupings of channels.
- **Decoders** — small programs to read and interpret binary data.

The XML representation is the sequential concatenation of the payload of every
block with group ID 0.  The standard SIE XML metadata preamble is:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<sie version="1.0" xmlns="http://www.somat.com/SIE">
<!-- SIE format standard definitions: -->
 <!-- SIE stream decoder: -->
 <decoder id="0">
  <loop>
   <read var="size" bits="32" type="uint" endian="big"/>
   <read var="group" bits="32" type="uint" endian="big"/>
   <read var="syncword" bits="32" type="uint" endian="big" value="0x51EDA7A0"/>
   <read var="payload" octets="{$size - 20}" type="raw"/>
   <read var="checksum" bits="32" type="uint" endian="big"/>
   <read var="size2" bits="32" type="uint" endian="big" value="{$size}"/>
  </loop>
 </decoder>
 <tag id="sie:xml_metadata" group="0" format="text/xml"/>

 <!-- SIE index block decoder:  v0=offset, v1=group -->
 <decoder id="1">
  <loop>
   <read var="v0" bits="64" type="uint" endian="big"/>
   <read var="v1" bits="32" type="uint" endian="big"/>
   <sample/>
  </loop>
 </decoder>
 <tag id="sie:block_index" group="1" decoder="1"/>

<!-- Stream-specific definitions begin here: -->
```

This preamble contains a self-describing description of the block structure
itself, plus the format of the block indexes.  The purpose of including this
preamble is to realize the design goal that the format be completely
self-describing.  Even without this document, looking at an SIE file in a text
editor will yield a good chance of understanding the basic format.

**XML merge behavior:** To support streaming data, the XML format supports
overriding and modifying metadata from earlier in the stream.  For example:

```xml
<!-- These two are equivalent: -->

<!-- Incremental form: -->
<ch id="42" name="test">
  <tag id="core:description">testing</tag>
</ch>
<ch id="42">
  <tag id="core:output_samples">74088</tag>
</ch>

<!-- Merged result: -->
<ch id="42" name="test">
  <tag id="core:description">testing</tag>
  <tag id="core:output_samples">74088</tag>
</ch>
```

This *merging* behavior allows adding information once it is known (e.g.,
the number of output samples is clearly not known until all samples are
written).

Some metadata is atomic and *replaces* instead (e.g. `tag` elements with
the same `id`).

| Element | Behavior | Key |
|---------|----------|-----|
| `ch` | merge | `id` |
| `dim` | merge | `index` |
| `test` | merge | `id` |
| `data` | replace | *(unique)* |
| `tag` | replace | `id` |
| `xform` | replace | *(unique)* |

> **Zig implementation note:** The XML merge engine is in `src/xml_merge.zig`.
> `XmlDefinition` handles the merge/replace logic according to the table above.
> The XML parser in `src/xml.zig` is a custom incremental parser that handles
> the SIE-specific preamble and unclosed `<sie>` element.

**Path shortcut expansion:** To reduce metadata size and require less state
from the writer, the attribute names `test`, `ch`, and `dim` are reserved for
all elements and produce hierarchy shortcuts:

```xml
<!-- Shortcut form: -->
<tag ch="2" dim="3" test="1" id="core:description">test</tag>

<!-- Expanded equivalent: -->
<test id="1">
  <ch id="2">
    <dim index="3">
      <tag id="core:description">test</tag>
    </dim>
  </ch>
</test>
```

**Inheritance:** The `ch` and `test` elements support inheritance via the
`base` attribute:

```xml
<ch id="2" name="old">
  <tag id="core:description">test</tag>
</ch>
<ch id="42" base="2" name="new"/>

<!-- Equivalent to: -->
<ch id="42" name="new">
  <tag id="core:description">test</tag>
</ch>
```

Inheritance is useful for creating many similar channels without excessive
duplication.

#### 2.3.2 Index blocks (group 1)

Index blocks, if present, allow an SIE file to be opened more quickly.  The
payload of an index block consists of a sequence of 64-bit offset and 32-bit
group pairs; each offset and group correspond to one of the blocks being
indexed.  Index blocks appear *after* the blocks which they are indexing,
allowing the SIE file to be streamed.

Each index block must cover a consecutive chunk of blocks ending with the last
block before the index block itself.  Index blocks present in another later
index block will be passed over to allow reindexing of a file.

> **Zig implementation note:** Index building is in `File.buildIndex()` in
> `src/file.zig`.  The `Writer` in `src/writer.zig` handles writing index
> blocks with proper offset tracking.

### 2.4 XML metadata grammar

#### 2.4.1 `<sie>` element

The top-level element of the SIE XML metadata.  The `version` attribute
specifies the SIE version (version 1.0 is described in this document).  Can
contain `tag`s, `decoder`s, channels (`ch`), and `test`s.

As an SIE stream is always capable of being appended to, the `<sie>` element
should never be explicitly closed.  For parsing purposes, consider `</sie>`
added to the end of the XML metadata.

#### 2.4.2 `<tag>` element

The generic element for application-level annotation.  Allows arbitrary text
as the tag name, and completely arbitrary values.

Tag values are stored either directly in the XML:

```xml
<tag id="core:sample_rate">500</tag>
```

Or as the contents of a group:

```xml
<tag id="setup_photo.1" group="27" format="image/jpeg"/>
```

For the group form, the value of the tag is the sequential concatenation of
the payload of every block with the specified group ID.

> **Zig implementation note:** Tags are modeled by the `Tag` struct in
> `src/tag.zig`.  String tags store their value directly; group-based tags
> store the group ID.  `Tag.isString()` and `Tag.isBinary()` distinguish the
> two forms.

#### 2.4.3 `<decoder>` element

Each decoder contains the machinery to decode the binary payload from a
particular data block.  The same decoder may be used by any number of different
channels.

**Variables:** Decoder variables can contain either numbers or arbitrary binary
data of unbounded size.  Variables are always initialized to zero upon entering
a decoder.  The special variables `v0`, `v1`, …, `vN` are used by the
`<sample>` operator to compose output result vectors.

**Expressions:** Expressions start with `{` and end with `}`.  Simple
arithmetic expressions are supported, with variable dereferencing via the `$`
prefix (e.g. `{$foo + 24}` is 24 plus the value of variable `foo`).

The decoder language operators are:

| Operator | Description |
|----------|-------------|
| `<if condition="EXPR">` | Conditional execution. Non-zero = execute contents. |
| `<loop>` | General iteration. Optional `var`, `start`, `end`, `increment` attributes. |
| `<read var="..." bits="N">` | Read *N* bits from payload. Types: `int`, `uint`, `float`, `raw`. Endian: `big` or `little`. |
| `<sample/>` | Emit output vector `[v0, v1, …, vN]` to the decoder's output queue. |
| `<seek from="..." offset="..."/>` | Move read position. `from`: `start`, `current`, or `end`. |
| `<set var="..." value="..."/>` | Assign a variable. |

The `<read>` operator's `type` attribute values:

| Type | Description |
|------|-------------|
| `int` | Two's complement signed integer (8, 16, 32, or 64 bits) |
| `uint` | Unsigned integer (8, 16, 32, or 64 bits) |
| `float` | IEEE-754 floating point (32 or 64 bits) |
| `raw` | No interpretation; unmodified binary buffer |

The `endian` attribute: `big` (network/Motorola order) or `little`
(Intel/x86 order).

> **Zig implementation note:** The decoder VM is in `src/decoder.zig` with 51
> opcodes.  The compiler (`src/compiler.zig`) translates XML decoder elements
> into bytecode.  The `DecoderMachine` executes bytecode and produces `Output`
> objects.  Expression parsing and register allocation are handled at compile
> time, not at runtime.

#### 2.4.4 `<ch>` element

The container for engineering-level data channels.  Contains `<dim>` elements
whose `<xform>`, `<data>`, and `<tag>` elements have all the information
necessary to convert raw decoder data vectors into engineering unit data.

Optional attributes:
- `name` — channel name
- `group` — single group associated with the channel
- `base` — inherit from another channel (see inheritance above)

#### 2.4.5 `<dim>` element

The definition for axis at `index`.  The first axis is index zero.  Decoder
data for the dimension comes from the `<dim>`'s `group` attribute, or if absent,
the enclosing `<ch>`'s `group` attribute.

#### 2.4.6 `<xform>` element

Defines a data transform:

- **Linear transform:** `scale` (slope) and `offset` (intercept) attributes.
- **Index mapping transform:** `index_ch` and `index_dim` attributes define
  an array lookup from another channel's dimension.

> **Zig implementation note:** Transforms are in `src/transform.zig`.  Linear
> transforms are applied as `value * scale + offset`.  Map transforms use a
> lookup table.  `Transform.setFromXformNode()` parses the XML `<xform>`
> element.

#### 2.4.7 `<data>` element

`<data decoder="D" v="V">` assigns element `vV` from decoder `D`'s output to
the containing dimension.  For example:

```xml
<dim index="2">
  <data decoder="7" v="3"/>
</dim>
```

assigns element `v3` of decoder 7's output to dimension 2.

### 2.5 Data rendering algorithm

To get the data model representation of an SIE channel's data:

1. For each dimension *d*, determine:
   - Which **group** (*g*) the data are read from
   - Which **decoder** (*D*) will decode the data
   - Which **v** of the decoder (*v_d*) maps to the dimension
   - Optionally, what **transform** (*x_d*) to apply

2. If any required information is missing, the channel is *abstract* and does
   not have data.  This is common for base channels that only hold common
   metadata for inheritance.

3. The rendering algorithm:

```
For each block b in the SIE stream with group ID g:
    Run decoder D on the payload of b, producing output vectors.
    For each decoder output vector V:
        Start building an output row r.
        For each dimension d in the channel:
            Let o = V[v_d].
            If transform x_d exists, apply it to o.
            Assign r[d] = o.
        Append row r to the channel data.
```

The result is the channel data in the universal SIE data model described in
section 2.1.

> **Zig implementation note:** This algorithm is implemented in
> `ChannelSpigot.get()` in `src/channel_spigot.zig`.  It reads blocks via the
> `Intake` interface, runs the `DecoderMachine`, and applies transforms from
> the `Transform` object — all wired together by `SieFile.attachSpigot()`.

---

## 3. Implementation

### 3.1 Writing SIE

The design of SIE is biased to be simple and efficient to write.  An SIE
writing implementation can output almost any binary format desired; the binary
data need only be packaged in self-contained format in SIE blocks and have the
appropriate SIE XML metadata defining how to interpret it.

As an example, consider an extremely simple data logger with only 128 bytes of
usable RAM.  Pre-computed XML metadata segments for the device's single output
format are stored in ROM.  To output SIE, the first XML section is output in a
group 0 block (incrementally, since the size is known ahead of time), then each
piece of variable metadata is computed and output as a group 0 block.  After
the XML is complete, the data is output in data blocks.

All CRCs can be written as zero (though constant XML sections could have
precomputed CRCs).  Index blocks are optional and would not be written in
this scenario.

**Writing guidelines:**

- Choose data representations that are self-contained (e.g. in
  `somat:histogram`, each row contains both the bin count and its position in
  engineering units).
- Choose block sizes that balance overhead (20 bytes per block; ~32 with
  indexes) against seekability (the block is the natural reading granularity).
- If at all possible, use CRCs on XML and index blocks even if data CRCs
  cannot be computed due to performance limitations.

> **Zig implementation note:** The `Writer` in `src/writer.zig` handles SIE
> block writing with automatic CRC-32 computation, XML/index group buffering,
> and auto-flush.

### 3.2 Reading SIE

On an abstract level, reading an SIE file is relatively easy.  However, writing
a program to read *all* SIE files is rather complicated.  A general reader must
contain at least:

- An XML parser
- A parser for the decoder expression language
- A decoder language interpreter or compiler
- A robust implementation of the data rendering algorithm
- An API general enough to deal with the wide variety of data representable
  in SIE

> **Zig implementation note:** libsie-zig provides all of the above:
> - XML parser: `src/xml.zig` (incremental, streaming)
> - Decoder compiler: `src/compiler.zig` (XML → bytecode)
> - Decoder VM: `src/decoder.zig` (51-opcode register machine)
> - Data rendering: `src/channel_spigot.zig` (full pipeline)
> - High-level API: `src/sie_file.zig` (`SieFile.open()` does everything)
>
> See [API_REFERENCE.md](API_REFERENCE.md) for the complete API documentation.

---

## License

> Copyright (C) 2005-2010 HBM Inc., SoMat Products
>
> This document is free software; you can redistribute it and/or modify it
> under the terms of version 2.1 of the GNU Lesser General Public License as
> published by the Free Software Foundation.
>
> This document is distributed in the hope that it will be useful, but WITHOUT
> ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
> FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
> details.
