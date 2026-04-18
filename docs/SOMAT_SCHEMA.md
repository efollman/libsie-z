# The `somat` SIE Schema, Version 1.0

> **Original authors:** Brian Downing & Chris LaReau, HBM, Inc. (December 2009)
>
> This document is ported from the original LaTeX source (`somat-schema.tex`).
> The original C library is available at
> [github.com/efollman/libsie-reference](https://github.com/efollman/libsie-reference)
> or from the [HBM/SoMat download archive](https://www.hbm.com/tw/2082/somat-download-archive/).
> The somat schema describes metadata and data representations specific to
> SoMat's line of data acquisition systems.  This schema is format-agnostic
> and applies identically to both the C and Zig implementations.

---

## 1. Introduction

This document describes the `somat` schema for the SIE format.  This schema is
used by SoMat's line of data acquisition systems.  In addition, some elements
of this schema may be applicable for more general use.  In almost all cases,
data files that reference this schema will also reference the `core` schema.

---

## 2. Metadata schema

### `somat:data_bits`

The number of bits of resolution of the channel data.

```xml
<tag id="somat:data_bits">16</tag>
```

### `somat:data_format`

The source data format of the channel data: `int`, `uint`, or `float`.

```xml
<tag id="somat:data_format">uint</tag>
```

### `somat:datamode_name`

The name of the datamode which produced the current SIE channel.

```xml
<tag id="somat:datamode_name">th1k</tag>
```

### `somat:datamode_type`

The datamode type.  Possible values:

- `time_history`
- `burst_history`
- `peak_valley`
- `peak_valley_slice`
- `event_slice`
- `time_at_level`
- `peak_valley_matrix/from_to`
- `peak_valley_matrix/range_mean`
- `peak_valley_matrix/range_only`
- `rainflow/from_to`
- `rainflow/range_mean`
- `rainflow/range_only`
- `message_log`

```xml
<tag id="somat:datamode_type">time_history</tag>
```

### `somat:input_channel`

The name of the input channel which produced the current SIE channel.

```xml
<tag id="somat:input_channel">bracket</tag>
```

### `somat:log`

The data acquisition system's log.  On the eDAQ, this is the same as what is
retrieved with the "FCS Setup | Upload FCS Log" command in TCE.  The log is
stored continuously and is as such not limited in size.

### `somat:rainflow_unclosed_cycles`

A sequence of ASCII-formatted floating-point numbers containing the unclosed
cycles stack from the rainflow counting algorithm.

```xml
<tag id="somat:rainflow_unclosed_cycles">
  9.992119789123535 20.02879905700684 -40.09270095825195
  40.0364990234375 -50.09659957885742 60.07699966430664
  -70.13710021972656 70.08080291748047 -80.14089965820312
  90.12129974365234 -110.1849975585938 150.2100067138672
  -120.1890029907227 190.2579956054688 -150.2330017089844
  230.3390045166016 -330.5 390.5650024414062 -681.0609741210938
  630.9199829101562 -590.89501953125 460.6900024414062
  -460.7139892578125 340.5130004882812 -320.4960021972656
  170.2510070800781 -210.3220062255859
</tag>
```

### `somat:tce_setup`

The .TCE setup file used to initialize the current test.

### `somat:version`

The version of the `somat` schema in use.  The version described in this
document is 1.0.

```xml
<tag id="somat:version">1.0</tag>
```

---

## 3. Data schemas

### `somat:sequential`

Used for representing time series numerical data sampled at a regular interval,
or the sequential output of various data reduction algorithms which dispose of
time information.  Each row represents a single data sample and the time or
sequence number of that sample.

Where time is preserved:

| SIE dim index | Data type | Scaled | Unscaled |
|---------------|-----------|--------|----------|
| 0 | Numeric | Time | Sample number |
| 1 | Numeric | Engineering data value | *undefined* |

Otherwise:

| SIE dim index | Data type | Scaled | Unscaled |
|---------------|-----------|--------|----------|
| 0 | Numeric | Sequence number | Sequence number |
| 1 | Numeric | Engineering data value | *undefined* |

The values of SIE dimension index 0, scaled or unscaled, have non-decreasing
ordering.

### `somat:message`

Used for representing non-numerical data sampled at irregular intervals.  Each
row represents a single message or event and the time of collection.

| SIE dim index | Data type | Scaled | Unscaled |
|---------------|-----------|--------|----------|
| 0 | Numeric | Time | *undefined* |
| 1 | Raw | Binary message data | *undefined* |

The values of SIE dimension index 0, scaled or unscaled, have non-decreasing
ordering.

### `somat:burst`

Used for representing time series numerical data sampled at a regular interval,
but where actual data collection is triggered by a triggering event.  Each row
represents a single data sample, the time that sample was collected, and the
relation between that sample and the event which triggered collection.

| SIE dim index | Data type | Scaled | Unscaled |
|---------------|-----------|--------|----------|
| 0 | Numeric | Time | Sample number |
| 1 | Numeric | Engineering data value | *undefined* |
| 2 | Numeric | Burst index | Burst index |

The values of SIE dimension index 0, scaled or unscaled, have non-decreasing
ordering.

For `somat:burst`, the *sample number* is absolute; i.e. if the first burst
data happened at the 50,000th sample collected, the sample number emitted from
SIE dimension 0 for that data would be 50,000, not 0.

The purpose of the *burst index* is so that the position of the burst trigger
can be known:

| Burst index | Meaning |
|-------------|---------|
| Integer *n* < 0 | Current sample is *n* samples before the burst trigger. |
| 0 | Current sample is the first sample in the burst trigger. |
| 0.5 | Current sample is a continuation of the (level-sensitive) burst trigger. |
| Integer *n* > 0 | Current sample is *n* samples after the burst trigger. |

### `somat:histogram`

Used for representing *n*-dimensional histogram data.  Each row represents a
single histogram bin's count and limits in all incoming dimensions.  The
histogram bins are not presented in any particular order, nor are empty bins
guaranteed to be present.

| SIE dim index | Data type | Scaled | Unscaled |
|---------------|-----------|--------|----------|
| 0 | Numeric | Bin count | *undefined* |
| *n* × 2 + 1 | Numeric | Lower bin limit for histogram dimension *n* | *undefined* |
| *n* × 2 + 2 | Numeric | Upper bin limit for histogram dimension *n* | *undefined* |

The number of histogram dimensions is equal to (*m* − 1) / 2, where *m* is the
number of SIE dimensions.

Overflow bins are explicitly specified: a negative overflow bin will have a
lower bin limit of −∞, while a positive overflow bin will have an upper bin
limit of +∞.

A data sample will fall into a bin if it is in the range [*lower*, *upper*);
the lower bound is closed while the upper bound is open.

If a bin is specified more than once, the last count is valid.  This allows
"streaming" histogram data.

> **Zig implementation note:** The `Histogram` struct in `src/histogram.zig`
> provides a convenience interface to access a histogram stored in this schema
> as an *n*-dimensional array rather than as a linear list of bins.
> `Histogram.fromChannel()` reads all data from a channel and builds the
> indexed structure.  `Histogram.getBin()` and `Histogram.setBin()` use
> multi-dimensional index arrays.  `Histogram.getNextNonzeroBin()` iterates
> over non-zero bins efficiently.

### `somat:rainflow`

This schema is identical to the `somat:histogram` data schema save for the
presence of a metadata tag `somat:rainflow_unclosed_cycles`.  This tag contains
a sequence of ASCII-formatted floating-point numbers containing the unclosed
cycles stack from the rainflow counting algorithm.  Note that the histogram
emitted by SIE has been closed; to open it again the rainflow algorithm must be
run in reverse with the unclosed cycles stack.

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
