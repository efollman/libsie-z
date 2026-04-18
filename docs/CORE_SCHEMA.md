# The `core` SIE Schema, Version 1.0

> **Original authors:** Brian Downing & Chris LaReau, HBM, Inc. (December 2009)
>
> This document is ported from the original LaTeX source (`core-schema.tex`).
> The original C library is available at
> [github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference)
> or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).
> The core schema defines standard metadata tags applicable to most engineering
> data.  This schema is format-agnostic and applies identically to both the C
> and Zig implementations.

---

## 1. Introduction

SIE provides a very flexible framework for describing almost any kind of data
and associated metadata.  However, some organization and standardization is
necessary so as to allow the widest number of programs to understand the widest
amount of data.

SIE tag names can belong to a particular metadata *schema*.  The schema part of
a tag name is the part preceding a colon.  For example, tag names beginning
with `core:` belong to the `core` schema described in this document.

Not all of the tags below will be present; in fact, none have to be.  For
maximum robustness, data reading applications should be able to work with the
absolute minimum set of tags possible.

In addition to metadata schemas, the `core:schema` tag indicates which *data
schema* is in use for the numerical and binary data output from a channel.
Referencing the correct data schema documentation will allow the data to be
correctly interpreted.

---

## 2. Metadata schema

### `core:description`

A free-form, natural-language description of the element it is contained in.

```xml
<tag id="core:description">
  A thermocouple mounted underneath the right front wheel bearing.
  This channel is gated to only collect data when the temperature is
  above 60 degrees C.
</tag>
```

### `core:elapsed_time`

The amount of time spent collecting the data in its container, in seconds.

```xml
<tag id="core:elapsed_time">160760.4</tag>
```

### `core:input_samples`

The number of data samples that were present before any data-reduction
algorithms were run.  For example, in a gated time history,
`core:input_samples` would contain the number of samples present before gating.

```xml
<tag id="core:input_samples">4760</tag>
```

### `core:label`

A short description intended for placing on a plot, graph, or table.  For a
dimension label, an expected usage would be for an axis label.

```xml
<tag id="core:label">Time</tag>
```

### `core:output_samples`

The number of data samples that are present after any data-reduction algorithms
were run; in other words, the number of data samples actually stored in the
channel.  For example, in a gated time history, `core:output_samples` would
contain the number of samples stored.

```xml
<tag id="core:output_samples">200</tag>
```

### `core:range_max`

The expected maximum bound for the container dimension.  Can be used to set
plot range boundaries.  This is *not* the maximum data value in the dimension.

```xml
<tag id="core:range_max">1000.0</tag>
```

### `core:range_min`

As `core:range_max`, but defines the minimum bound.

```xml
<tag id="core:range_min">-1000.0</tag>
```

### `core:sample_rate`

The sample rate, in Hz, of the data in its container.

```xml
<tag id="core:sample_rate">2500</tag>
```

### `core:schema`

Defines what data schema is in use for this channel.  The data schema defines
how to interpret the data output of the channel.  For example, if `core:schema`
was `somat:sequential`, you would go to the `somat` schema document and look up
the `sequential` data schema.

```xml
<tag id="core:schema">somat:sequential</tag>
```

### `core:setup_name`

The name of the setup under which the data in the container has been collected,
if applicable.

```xml
<tag id="core:setup_name">bearing_temp</tag>
```

### `core:start_time`

The time that data collection started for the container, in ISO 8601 format.

```xml
<tag id="core:start_time">2007-01-10T11:49:34-0600</tag>
```

### `core:stop_time`

As `core:start_time`, but contains the time when data collection stopped.

```xml
<tag id="core:stop_time">2007-01-10T12:12:06-0600</tag>
```

### `core:test_count`

The sequence number of the container test.  For example, on a data acquisition
system where a test can be repeated multiple times, the first test run would
have `core:test_count` of 1, the second would be 2.

```xml
<tag id="core:test_count">14</tag>
```

### `core:units`

The units of the container dimension (e.g. seconds, millivolts, microstrain).
For unitless dimensions (e.g. counts) this tag is absent.

```xml
<tag id="core:units">seconds</tag>
```

### `core:version`

The version of the `core` schema in use.  The version described in this
document is 1.0.

```xml
<tag id="core:version">1.0</tag>
```

---

## 3. Data schemas

Version 1.0 of the `core` schema does not offer any data schemas.  See
[SOMAT_SCHEMA.md](SOMAT_SCHEMA.md) for output schemas for typical engineering
data formats.

---

> **Zig implementation note:** All core tags are accessed through the standard
> `Tag` API.  Use `channel.findTag("core:schema")` to determine the data
> schema for a channel, `dimension.findTag("core:units")` for units, etc.
> Tag values are always returned as string slices — no allocation or freeing
> needed (unlike the C API's `sie_tag_get_value()` which required
> `sie_free()`).

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
