# node-clipper

A node.js wrapper for the [Clipper polygon clipper](http://www.angusj.com/delphi/clipper.php) (and helper) library

## Project State

The wrapper exposes a lot of useful functions of clipper library to node.js. The function "minimum" is an outstanding feature to compute the most inner point of complex polygons with inner and outer borders. It is very useful to get the perfect point for a placemark inside the polygon.


## Data structures

### Value

A value is a javascript number. In clipper library all numbers are integer. So if floating point numbers are used it will be converted in the function call. Mandantory the functions have to be called with number type "integer" | "double". Double values should not be greater than ~1000 because of the upscaling to an integer.

### Point

A point is an array of two values:
```javascript
var p= [ v1, v2 ];
```

### Polygon

A polygon is an array of Points:
```javascript
var poly= [ [ v1, v2 ], [ v3, v4 ], [ v5, v6 ], [ v7, v8 ] ];
```

### Polyshape

The polyshape structure is an array of polygons. The first element in the array is the outer border polygon (points of this polygon clockwise oriented) followed by any number of inner border polygons:
```javascript
var polyShape= [
  [ [ v1, v2 ], [ v3, v4 ], [ v5, v6 ], [ v7, v8 ] ],
  [ [ v9, v10 ], [ v11, v12 ], [ v13, v14 ], [ v15, v16 ] ]
];
```



## Bindings

### Offset

See the [clipper documentation for Offset](http://www.angusj.com/delphi/clipper/documentation/Docs/Units/ClipperLib/Functions/OffsetPaths.htm))
in order to get an idea of how this is used.

```javascript
clipper = require('../build/Release/clipper')

/*a
  ## Offset
  * arguments: flattened_coordinate_path, offset
  * where: offset can be positive or negative (negative will go inside the polygon, positive will expand outside)
*/
flattened_coordinate_path = [0, 0, 0, 10, 10, 10, 10, 0]

// A negative offset gets an offset inside our Polygon:
inner_highlight_path = clipper.offset(flattened_coordinate_path, -2)
console.log(inner_highlight_path)
// returns [ [ 2, 2, 8, 2, 8, 8, 2, 8 ] ]

// A positive offset gets an offset outside our Polygon:
outter_highlight_path = clipper.offset(flattened_coordinate_path, 2)
console.log(outter_highlight_path)
// returns [ [ -2, 12, -2, -2, 12, -2, 12, 12 ] ]
```
