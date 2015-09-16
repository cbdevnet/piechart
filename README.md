# piechart
Create SVG pie charts directly from the command line!

## Usage
**piechart** expects one line of data per slice of the chart. Slice properties are separated by a `delimiter`
(Default: `,`), with their order specified by the `--order` argument (Default: `value,color,legend`).
Simply pipe your data into `piechart` or save it to a file and specify that as an argument.

## Arguments
Argument                | Effect                                                              | Default
------------------------|---------------------------------------------------------------------|-----------------
--delimiter *delimiter* | Set input property delimiter                                        | `,`
--order *property-list* | Set input property order (see below)                                | `value,color,legend`
--color *color-spec*    | Set slice default fill color (may be overridden by input data)      | `white`
--border *color-spec*   | Set slice border color                                              | `black`
--explode *offset*      | Set slice default explode offset (may be overridden by input data)  | `0`
--no-legend             | Disable legend text output                                          | -
*inputfile*             | Read data from *inputfile* instead of the standard input            | `stdin`

### Specifying property order
The property order can be specified by using the `--order` argument to **piechart**, supplying a comma-separated list
of any of the property keywords below

Keyword       | Effect
--------------|--------------------------------------------------------
**ignore**    | Ignore the column
**value**     | Column specifies the absolute value of the slice
**legend**    | Column contains slice legend
**color**     | Column contains slice fill color
**explode**   | Column contains slice explode offset

### Specifying colors
Since piechart outputs SVG data directly, all HTML/CSS colors may be used. This includes color names like 
`blue` and `red` as well as hex-encoded colors such as `#12ab34`.

## Why
Because there seems to be no simple tool for creating basic pie charts from the command line,
or at least my searches did not turn up anything useful. If you know of anything, please tell me :)

## Example output
[Output of](tests/diag3.svg) `piechart tests/diag3 --order value,explode,color,legend > tests/diag3.svg`

## License
See [LICENSE.txt](LICENSE.txt)
