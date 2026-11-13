#!/usr/bin/env python3
"""Writes the small skinned rigs of the in-game checks, as glTF files with their buffers embedded.

Each rig is a column of three bones standing on the origin, one metre each, skinned to a box 0.2 m wide:
- Chain: the bones `root`, `mid` and `tip`, at rest without rotation, and the clip `Bend` (one second: the root rises
  and turns, `mid` bends about Z, `tip` about X);
- ChainRenamed: the same rig, its bones named `Hip`, `Spine` and `Head`;
- ChainTurned: the same column, each bone's rest turned another way (its joints stand where Chain's do);
- ChainScaled: Chain with `mid` scaled at rest, which a clip of Chain cannot play;
- ChainReparented: Chain with `tip` under `root`, which a clip of Chain cannot play either;
- Post: a post 1.5 m tall (the bone `post`, up to the bone `post_top`), and a rope of three bones (`rope_1`, `rope_2`,
  `rope_3`, 0.4 m each) hanging from its top, 0.15 m beside it, for the spring bones: every rest unturned, so the
  rope's bones point down along their parents' offsets, not along +Y.

Run it from anywhere: python3 Tests/GameChecks/Tools/make_rigs.py
"""

import base64
import json
import math
import os
import struct

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "Models", "Rigs")

# Quaternions (x, y, z, w) and 4x4 column-major matrices, as glTF writes them

def q_axis(axis, degrees):
	half = math.radians(degrees) * 0.5
	s = math.sin(half)
	return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))

def q_mul(a, b):
	ax, ay, az, aw = a
	bx, by, bz, bw = b
	return (aw * bx + ax * bw + ay * bz - az * by,
		aw * by - ax * bz + ay * bw + az * bx,
		aw * bz + ax * by - ay * bx + az * bw,
		aw * bw - ax * bx - ay * by - az * bz)

def q_conj(q):
	return (-q[0], -q[1], -q[2], q[3])

def q_rotate(q, v):
	x, y, z = q_mul(q_mul(q, (v[0], v[1], v[2], 0.0)), q_conj(q))[:3]
	return (x, y, z)

def m_compose(t, q, s):
	x, y, z, w = q
	r = [
		[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
		[2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
		[2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
	]
	m = [[r[i][j] * s[j] for j in range(3)] + [t[i]] for i in range(3)] + [[0.0, 0.0, 0.0, 1.0]]
	return m

def m_mul(a, b):
	return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]

def m_inverse_rigid_scaled(m):
	# Inverse of a matrix made of translation, rotation and scale, through its 3x3 inverse
	a = [row[:3] for row in m[:3]]
	det = (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
		- a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
		+ a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))
	inv = [[0.0] * 3 for _ in range(3)]
	inv[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) / det
	inv[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / det
	inv[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / det
	inv[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / det
	inv[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / det
	inv[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / det
	inv[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / det
	inv[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / det
	inv[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / det
	t = [m[0][3], m[1][3], m[2][3]]
	it = [-sum(inv[i][k] * t[k] for k in range(3)) for i in range(3)]
	return [inv[i] + [it[i]] for i in range(3)] + [[0.0, 0.0, 0.0, 1.0]]

def column_major(m):
	return [m[i][j] for j in range(4) for i in range(4)]

IDENTITY = (0.0, 0.0, 0.0, 1.0)

# The joints of the column, in world space, and the bones' names in Chain
JOINTS = [(0.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 2.0, 0.0)]
NAMES = ["root", "mid", "tip"]

def make_rig(names, rest_rotations, parents=(None, 0, 1), scales=None, clip=False):
	"""A rig whose joints stand at JOINTS, each with its own rest rotation (world space), as a glTF document"""
	scales = scales or [(1.0, 1.0, 1.0)] * 3

	# Each joint's global rest, then its local rest relative to its parent
	globals_ = []
	locals_ = []
	for i in range(3):
		p = parents[i]
		g_rot = rest_rotations[i]
		if p is None:
			t_local = JOINTS[i]
			r_local = g_rot
		else:
			parent_rot = rest_rotations[p]
			offset = tuple(JOINTS[i][k] - JOINTS[p][k] for k in range(3))
			t_local = q_rotate(q_conj(parent_rot), offset)
			# A parent's scale is undone, so that the joint stands where it should
			ps = scales[p]
			t_local = (t_local[0] / ps[0], t_local[1] / ps[1], t_local[2] / ps[2])
			r_local = q_mul(q_conj(parent_rot), g_rot)
		locals_.append((t_local, r_local, scales[i]))

	for i in range(3):
		t, r, s = locals_[i]
		m = m_compose(t, r, s)
		p = parents[i]
		globals_.append(m if p is None else m_mul(globals_[p], m))

	# The box: rings of four vertices at y = 0, 1, 2 and 3, each bound to the bone at or below it
	half = 0.1
	positions, joints, weights = [], [], []
	for ring, y in enumerate([0.0, 1.0, 2.0, 3.0]):
		joint = min(ring, 2)
		for x, z in [(-half, -half), (half, -half), (half, half), (-half, half)]:
			positions += [x, y, z]
			joints += [joint, 0, 0, 0]
			weights += [1.0, 0.0, 0.0, 0.0]
	indices = []
	for ring in range(3):
		a, b = ring * 4, ring * 4 + 4
		for k in range(4):
			k2 = (k + 1) % 4
			indices += [a + k, a + k2, b + k2, a + k, b + k2, b + k]
	indices += [0, 2, 1, 0, 3, 2, 12, 13, 14, 12, 14, 15]

	inverse_binds = []
	for m in globals_:
		inverse_binds += column_major(m_inverse_rigid_scaled(m))

	blob = bytearray()
	views, accessors = [], []

	def add(data, fmt, count, component, kind, target=None, minmax=None):
		while len(blob) % 4:
			blob.append(0)
		offset = len(blob)
		blob.extend(struct.pack("<%d%s" % (len(data), fmt), *data))
		view = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset}
		if target:
			view["target"] = target
		views.append(view)
		accessor = {"bufferView": len(views) - 1, "componentType": component, "count": count, "type": kind}
		if minmax:
			accessor["min"], accessor["max"] = minmax
		accessors.append(accessor)
		return len(accessors) - 1

	FLOAT, USHORT, UBYTE = 5126, 5123, 5121
	a_pos = add(positions, "f", 16, FLOAT, "VEC3", 34962, ([-half, 0.0, -half], [half, 3.0, half]))
	a_joints = add(joints, "B", 16, UBYTE, "VEC4", 34962)
	a_weights = add(weights, "f", 16, FLOAT, "VEC4", 34962)
	a_indices = add(indices, "H", len(indices), USHORT, "SCALAR", 34963)
	a_ibm = add(inverse_binds, "f", 3, FLOAT, "MAT4")

	# Nodes: 0 Armature, 1..3 the joints, 4 the mesh
	nodes = [{"name": "Armature", "children": [1 + i for i in range(3) if parents[i] is None]}]
	for i in range(3):
		t, r, s = locals_[i]
		node = {"name": names[i], "translation": list(t), "rotation": list(r)}
		if s != (1.0, 1.0, 1.0):
			node["scale"] = list(s)
		children = [1 + c for c in range(3) if parents[c] == i]
		if children:
			node["children"] = children
		nodes.append(node)
	nodes.append({"name": "Body", "mesh": 0, "skin": 0})

	document = {
		"asset": {"version": "2.0", "generator": "Tests/GameChecks/Tools/make_rigs.py"},
		"scene": 0,
		"scenes": [{"nodes": [0, 4]}],
		"nodes": nodes,
		"meshes": [{"name": "Body", "primitives": [{
			"attributes": {"POSITION": a_pos, "JOINTS_0": a_joints, "WEIGHTS_0": a_weights},
			"indices": a_indices,
		}]}],
		"skins": [{"joints": [1, 2, 3], "inverseBindMatrices": a_ibm, "skeleton": 1}],
	}

	if clip:
		# Bend: one second, keys at 0, 0.5 and 1, in the rig's own rest frames (here, none)
		times = [0.0, 0.5, 1.0]
		a_times = add(times, "f", 3, FLOAT, "SCALAR", None, ([0.0], [1.0]))
		root_t = [0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0]
		a_root_t = add(root_t, "f", 3, FLOAT, "VEC3")
		root_r = list(IDENTITY) + list(q_axis((0.0, 1.0, 0.0), 30.0)) + list(IDENTITY)
		a_root_r = add(root_r, "f", 3, FLOAT, "VEC4")
		mid_r = list(IDENTITY) + list(q_axis((0.0, 0.0, 1.0), 45.0)) + list(IDENTITY)
		a_mid_r = add(mid_r, "f", 3, FLOAT, "VEC4")
		tip_r = list(IDENTITY) + list(q_axis((1.0, 0.0, 0.0), -60.0)) + list(IDENTITY)
		a_tip_r = add(tip_r, "f", 3, FLOAT, "VEC4")
		document["animations"] = [{
			"name": "Bend",
			"samplers": [
				{"input": a_times, "output": a_root_t, "interpolation": "LINEAR"},
				{"input": a_times, "output": a_root_r, "interpolation": "LINEAR"},
				{"input": a_times, "output": a_mid_r, "interpolation": "LINEAR"},
				{"input": a_times, "output": a_tip_r, "interpolation": "LINEAR"},
			],
			"channels": [
				{"sampler": 0, "target": {"node": 1, "path": "translation"}},
				{"sampler": 1, "target": {"node": 1, "path": "rotation"}},
				{"sampler": 2, "target": {"node": 2, "path": "rotation"}},
				{"sampler": 3, "target": {"node": 3, "path": "rotation"}},
			],
		}]

	document["buffers"] = [{
		"byteLength": len(blob),
		"uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii"),
	}]
	document["bufferViews"] = views
	document["accessors"] = accessors
	return document

def make_post():
	"""The post and its rope, as a glTF document"""
	# Bones: their joints in world space, their parents; all rests unturned
	joints = [(0.0, 0.0, 0.0), (0.0, 1.5, 0.0), (0.15, 1.5, 0.0), (0.15, 1.1, 0.0), (0.15, 0.7, 0.0)]
	names = ["post", "post_top", "rope_1", "rope_2", "rope_3"]
	parents = [None, 0, 1, 2, 3]

	# Two boxes, each a column of rings bound to the bone at or above them: the post (0.07 m either side), the rope
	# (0.02 m either side)
	positions, bones, indices = [], [], []
	def column(x, half, rings):
		first = len(positions) // 3
		for y, bone in rings:
			for dx, dz in [(-half, -half), (half, -half), (half, half), (-half, half)]:
				positions.extend([x + dx, y, dz])
				bones.append(bone)
		for ring in range(len(rings) - 1):
			a, b = first + ring * 4, first + ring * 4 + 4
			for k in range(4):
				k2 = (k + 1) % 4
				indices.extend([a + k, a + k2, b + k2, a + k, b + k2, b + k])
		last = first + (len(rings) - 1) * 4
		indices.extend([first, first + 2, first + 1, first, first + 3, first + 2, last, last + 1, last + 2, last, last + 2, last + 3])
	column(0.0, 0.07, [(0.0, 0), (1.5, 0)])
	column(0.15, 0.02, [(1.5, 2), (1.1, 3), (0.7, 4), (0.3, 4)])

	count = len(positions) // 3
	joint_data, weights = [], []
	for bone in bones:
		joint_data.extend([bone, 0, 0, 0])
		weights.extend([1.0, 0.0, 0.0, 0.0])

	inverse_binds = []
	for joint in joints:
		inverse_binds.extend(column_major(m_compose((-joint[0], -joint[1], -joint[2]), IDENTITY, (1.0, 1.0, 1.0))))

	blob = bytearray()
	views, accessors = [], []
	def add(data, fmt, count_, component, kind, target=None, minmax=None):
		while len(blob) % 4:
			blob.append(0)
		offset = len(blob)
		blob.extend(struct.pack("<%d%s" % (len(data), fmt), *data))
		view = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset}
		if target:
			view["target"] = target
		views.append(view)
		accessor = {"bufferView": len(views) - 1, "componentType": component, "count": count_, "type": kind}
		if minmax:
			accessor["min"], accessor["max"] = minmax
		accessors.append(accessor)
		return len(accessors) - 1

	FLOAT, USHORT, UBYTE = 5126, 5123, 5121
	xs, ys, zs = positions[0::3], positions[1::3], positions[2::3]
	a_pos = add(positions, "f", count, FLOAT, "VEC3", 34962, ([min(xs), min(ys), min(zs)], [max(xs), max(ys), max(zs)]))
	a_joints = add(joint_data, "B", count, UBYTE, "VEC4", 34962)
	a_weights = add(weights, "f", count, FLOAT, "VEC4", 34962)
	a_indices = add(indices, "H", len(indices), USHORT, "SCALAR", 34963)
	a_ibm = add(inverse_binds, "f", len(joints), FLOAT, "MAT4")

	# Nodes: 0 Armature, 1..5 the joints, 6 the mesh
	nodes = [{"name": "Armature", "children": [1]}]
	for i, name in enumerate(names):
		p = parents[i]
		offset = joints[i] if p is None else tuple(joints[i][k] - joints[p][k] for k in range(3))
		node = {"name": name, "translation": [round(v, 6) for v in offset], "rotation": list(IDENTITY)}
		children = [1 + c for c in range(len(names)) if parents[c] == i]
		if children:
			node["children"] = children
		nodes.append(node)
	nodes.append({"name": "Body", "mesh": 0, "skin": 0})

	return {
		"asset": {"version": "2.0", "generator": "Tests/GameChecks/Tools/make_rigs.py"},
		"scene": 0,
		"scenes": [{"nodes": [0, 6]}],
		"nodes": nodes,
		"meshes": [{"name": "Body", "primitives": [{
			"attributes": {"POSITION": a_pos, "JOINTS_0": a_joints, "WEIGHTS_0": a_weights},
			"indices": a_indices,
		}]}],
		"skins": [{"joints": [1, 2, 3, 4, 5], "inverseBindMatrices": a_ibm, "skeleton": 1}],
		"buffers": [{
			"byteLength": len(blob),
			"uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(blob)).decode("ascii"),
		}],
		"bufferViews": views,
		"accessors": accessors,
	}

def write(name, document):
	os.makedirs(OUT, exist_ok=True)
	with open(os.path.join(OUT, name + ".gltf"), "w", newline="\n") as f:
		json.dump(document, f, indent=1)
		f.write("\n")

UNTURNED = [IDENTITY, IDENTITY, IDENTITY]
TURNED = [q_axis((0.0, 1.0, 0.0), 90.0), q_mul(q_axis((0.0, 0.0, 1.0), 30.0), q_axis((1.0, 0.0, 0.0), -90.0)), q_axis((1.0, 0.0, 0.0), 180.0)]

write("Chain", make_rig(NAMES, UNTURNED, clip=True))
write("ChainRenamed", make_rig(["Hip", "Spine", "Head"], UNTURNED))
write("ChainTurned", make_rig(NAMES, TURNED))
write("ChainScaled", make_rig(NAMES, UNTURNED, scales=[(1.0, 1.0, 1.0), (1.5, 1.5, 1.5), (1.0, 1.0, 1.0)]))
write("ChainReparented", make_rig(NAMES, UNTURNED, parents=(None, 0, 0)))
write("Post", make_post())
