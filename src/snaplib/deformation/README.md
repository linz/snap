# Deformation evaluation core

Shared low-level evaluation core for GGRXF and LINZDEF deformation models: affine grid indexing,
bilinear interpolation, multi-resolution containment and priority, and GGXF-style time functions.

## Terminology

- **Layer** — one summed, independent contribution to total deformation (one GGRXF netCDF group).
  Every layer's own contribution is computed independently and simply added to every other
  layer's; a query never depends on how many layers exist or what's in them. Comes in two kinds:
  `ScalarLayer` (root grids plus a shared, summed set of time functions) and `RaggedLayer` (root
  ragged grids, no shared time function).

- **Grid** — one node in a layer's own resolution hierarchy: a single affine-indexed rectangle of
  nodes, plus (for scalar grids) whichever finer child grids nest inside it. A **root grid** is one
  directly under a layer, with no parent. Ragged grids never nest in any real file today — every
  one is a flat root-level sibling, even though the file format permits nesting in principle.

- **Cell** — the single bilinear cell one query point resolves into *within one grid*: the base
  corner `(bi, bj)`, its diagonal opposite `(bi+1, bj+1)`, and the fractional weights `(fi, fj)`
  between them. Produced by `GridGeometry::locate()` — identical for both grid types; what differs
  between them is only what happens with a `Cell` once it's found (a scalar grid fetches 4 raw
  values then scales once; a ragged grid evaluates each of the 4 corners' own segment chains at
  the query date first, then interpolates).

- **Node** — one lattice point within a grid, indexed `(i, j)`, `0 <= i < iNodeCount`,
  `0 <= j < jNodeCount`. Holds either a scalar grid's own raw stored value per axis, or (ragged) its
  own independent per-component segment chain.

- **Leaf** — one accepted (undivided) tile from a quadtree-tiling process over dense, irregularly
  shaped point coverage. In the file, each leaf becomes one root ragged grid (see "Grid" above) —
  "leaf" and "root ragged grid" name the same object, just from whichever side of the discussion
  (tiling vs. file structure) you're looking from.
