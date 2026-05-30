---
name: models-reference
description: Model-specific physics setup, naming conventions, known behaviors
type: reference
---

## ikaros-uniform
- 510 bodies (34 mode0, 476 mode1), 846 joints, 655 bones, 105k vertices
- Skirt: 340 box bodies (裙_X_Y), degree=4 grid, mass=10, mode=1. Connected to 下半身 (+ vertical/horizontal neighbors). Rotation limits (0,0,0) — locked.
- Breast: 6 clothLike (左胸上/下, 右胸上/下, 左胸上2, 右胸上2) with rotation springs kRot=250
- Ponytail: 马尾_0-18 chains, rotation limits (0,0,0) — rigid rods
- modelScale ≈ 0.099

## ikaros-origin
- 255 bodies (20 mode0, 235 mode1), 377 joints
- Breast: mode-0 control bodies (左胸操作) + mode-1 auxiliary spheres (左AH1 r=0.3, 左AH2 r=0.3)
- Breast capsule: 左胸操作接続 (r=0.1, h=1.515, rot=(0, 1.57, -1.57)) — connects left breast chain
- modelScale ≈ 0.099

## 安比 (Anby)
- 125 bodies (39 mode0, 6 mode1, 80 mode2), 153 joints
- Only model with mode-2 skirts (裙_X_Y). Mass=0.7-1.0, box shapes
- Breast: 6 clothLike with rotation springs kRot=250

## aqua-sailor
- 106 bodies, 103 joints
- Skirt: スカート1/2/3/... bodies, mode-0 parent (スカート親) with degree=8. Child bodies mass=0.1.
- Has rotation springs on skirt joints (kRot=20-30)
- modelScale ≈ 0.1

## chloe / marine-jk1
- 168/221 bodies respectively
- Skirt: スカート_0_0 etc., mode-0 wall (スカート壁) or parent (スカート親)
- Rotation springs kRot=10, light mass (0.3)

## Skirt Comparison
| Model | Naming | Mode | Mass | Structure |
|-------|--------|------|------|-----------|
| ikaros-uniform | 裙_X_Y | 1 | 10.0 | Grid (deg=4) |
| ikaros-origin | 裙_X_Y | 1 | 1.0 | Fan |
| 安比 | 裙_X_Y | 2 | 0.7-1.0 | Fan |
| aqua-sailor | スカートX-Y | 1 | 0.1 | Fan + mode-0 parent |
| chloe/marine | スカート_X_Y | 1 | 0.3 | Fan + mode-0 wall |
