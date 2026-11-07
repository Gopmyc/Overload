# Coordinates

- Y is up and forward is +Z. A character facing +Z has its left on +X and its right on −X.
- The yaw of a direction `v` is `math.atan(v.x, v.z)` (Lua 5.4: `math.atan` takes two arguments).
- `Quaternion.new(Vector3.new(pitch, yaw, roll))` takes degrees and composes Z · Y · X: roll, then yaw, then pitch
  applied first. An attachment measured in game is solved in that order.
- `Vector3.Right()` is +X, which is the screen's left for a camera looking down +Z.
- A camera's right is `forward × up`.
