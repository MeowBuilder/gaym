# Unity Map Exporter

## 파일 구조
```
UnityExporter/
├── EnemySpawnPoint.cs              → Assets/Scripts/
├── PlayerSpawnPoint.cs             → Assets/Scripts/
└── Editor/
    ├── EnemySpawnPointEditor.cs    → Assets/Editor/
    ├── PlayerSpawnPointEditor.cs   → Assets/Editor/
    └── MapExporter.cs              → Assets/Editor/
```

## Unity 씬 설정

### 태그 설정 (Edit > Project Settings > Tags)
| 태그 | 용도 |
|------|------|
| `Room` | 방 경계 오브젝트 |
| `Obstacle` | 충돌 처리할 벽/장애물 (Collider 필수) |
| `MapMesh` | 렌더링용 맵 오브젝트 (MeshFilter 필수) → OBJ 파일로 추출 |

### 씬 구성 예시
```
Scene
├── PlayerSpawnPoint         [PlayerSpawnPoint 컴포넌트]
├── Room_0                   [태그: Room]
│   ├── Floor                [태그: MapMesh]  ← OBJ 추출
│   ├── Wall_North           [태그: MapMesh + Obstacle + Collider]  ← OBJ + 충돌
│   ├── Pillar               [태그: MapMesh]  ← OBJ 추출
│   ├── NavMeshSurface       [NavMesh 베이크용]
│   ├── SpawnPoint_1         [EnemySpawnPoint: Goblin x2]
│   └── SpawnPoint_2         [EnemySpawnPoint: Archer x1]
└── Room_1                   [태그: Room]
    └── ...
```

> **참고**: 같은 Mesh를 공유하는 오브젝트(예: 같은 Wall 프리팹 여러 개)는
> OBJ 파일을 하나만 생성하고 map.json에서 경로로 참조합니다.

## 익스포트 방법

1. Unity 메뉴 → **Tools > Map Exporter**
2. 출력 경로 설정 (기본: `Assets/MapData/map.json`)
3. 익스포트 항목 체크
4. **Export to JSON** 버튼 클릭

## 출력 파일 구조

```
MapData/
├── map.json          ← 배치 정보 전체
└── meshes/           ← OBJ 파일들 (MapMesh 태그 오브젝트당 고유 메시 1개)
    ├── Floor.obj
    ├── Wall.obj
    └── Pillar.obj
```

## map.json 구조

```json
{
  "playerSpawn": { "position": [x,y,z], "rotationY": 0.0 },
  "rooms": [
    { "id": 0, "name": "Room_0", "boundsMin": [...], "boundsMax": [...], "center": [...] }
  ],
  "enemySpawns": [
    {
      "presetName": "Goblin", "count": 2, "activationDelay": 0.0,
      "position": [x,y,z], "rotationY": 0.0,
      "stats": { "maxHP": 100, "moveSpeed": 5, "attackRange": 2, "attackCooldown": 1, "detectionRange": 50 },
      "attackType": "Melee",
      "indicator": { "type": "Circle", "rushDistance": 0, "hitRadius": 2, "coneAngle": 0 },
      "visual": { "meshPath": "", "animationPath": "", "scale": [1,1,1], "color": [255,0,0,255] },
      "animClips": { "idle": "idle", "chase": "Run_Forward", "attack": "Combat_Unarmed_Attack", ... }
    }
  ],
  "obstacles": [
    { "name": "Wall_N", "center": [x,y,z], "size": [w,h,d] }
  ],
  "mapObjects": [
    {
      "name": "Wall_North_1",
      "meshFile": "meshes/Wall.obj",
      "position": [x,y,z],
      "rotation": [qx,qy,qz,qw],
      "scale": [1,1,1]
    }
  ],
  "navMesh": {
    "vertices": [[x,y,z], ...],
    "indices": [0, 1, 2, ...]
  }
}
```

## 좌표계 변환
Unity(좌수계) → DirectX(우수계): **Z축 부호 반전** 자동 적용됨
