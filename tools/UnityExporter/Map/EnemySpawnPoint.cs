using UnityEngine;

/// <summary>
/// 적 스폰 포인트. 씬에 빈 게임오브젝트를 만들고 이 컴포넌트를 붙여서 사용.
/// C++ EnemySpawnData / EnemyStats / AttackIndicatorConfig 와 1:1 매핑.
/// </summary>
public class EnemySpawnPoint : MonoBehaviour
{
    // ─────────────────────────────────────────
    //  기본 정보
    // ─────────────────────────────────────────
    [Header("기본 정보")]
    [Tooltip("C++ EnemyPreset 이름과 동일하게 설정 (예: Goblin, Archer)")]
    public string presetName = "Goblin";

    [Tooltip("이 위치에서 스폰할 적 수")]
    [Range(1, 10)]
    public int count = 1;

    [Tooltip("방 진입 후 스폰까지 대기 시간 (초)")]
    [Range(0f, 10f)]
    public float activationDelay = 0f;

    // ─────────────────────────────────────────
    //  전투 스탯  (C++ EnemyStats)
    // ─────────────────────────────────────────
    [Header("전투 스탯")]
    [Tooltip("최대 체력")]
    [Range(1f, 10000f)]
    public float maxHP = 100f;

    [Tooltip("이동 속도")]
    [Range(0.1f, 30f)]
    public float moveSpeed = 5f;

    [Tooltip("공격 시작 거리 (이 범위 안에 들어오면 공격)")]
    [Range(0.5f, 20f)]
    public float attackRange = 2f;

    [Tooltip("공격 쿨다운 (초)")]
    [Range(0.1f, 10f)]
    public float attackCooldown = 1f;

    [Tooltip("플레이어 감지 거리")]
    [Range(1f, 100f)]
    public float detectionRange = 50f;

    // ─────────────────────────────────────────
    //  공격 타입
    // ─────────────────────────────────────────
    [Header("공격 타입")]
    [Tooltip("C++ 공격 Behavior 와 매핑됨")]
    public AttackType attackType = AttackType.Melee;

    // ─────────────────────────────────────────
    //  공격 인디케이터  (C++ AttackIndicatorConfig)
    // ─────────────────────────────────────────
    [Header("공격 인디케이터")]
    [Tooltip("공격 예고 표시 타입")]
    public IndicatorType indicatorType = IndicatorType.None;

    [Tooltip("돌진 거리 (RushFront / RushAoE 전용)")]
    [Range(0f, 30f)]
    public float rushDistance = 0f;

    [Tooltip("피격 반경 (Circle / RushCircle 전용)")]
    [Range(0f, 20f)]
    public float hitRadius = 0f;

    [Tooltip("콘 각도 (RushCone 전용, 도 단위)")]
    [Range(0f, 360f)]
    public float coneAngle = 0f;

    // ─────────────────────────────────────────
    //  시각적 설정  (C++ EnemySpawnData)
    // ─────────────────────────────────────────
    [Header("시각적 설정")]
    [Tooltip("메시 경로 (비어있으면 큐브 사용)")]
    public string meshPath = "";

    [Tooltip("애니메이션 경로 (비어있으면 애니메이션 없음)")]
    public string animationPath = "";

    public Vector3 scale = Vector3.one;

    public Color color = Color.white;

    // ─────────────────────────────────────────
    //  애니메이션 클립 이름  (C++ EnemyAnimationConfig)
    // ─────────────────────────────────────────
    [Header("애니메이션 클립")]
    public string clipIdle   = "idle";
    public string clipChase  = "Run_Forward";
    public string clipAttack = "Combat_Unarmed_Attack";
    public string clipStagger = "Combat_Stun";
    public string clipDeath  = "Death";

    // ─────────────────────────────────────────
    //  씬 뷰 기즈모
    // ─────────────────────────────────────────
    void OnDrawGizmos()
    {
        // 스폰 위치 - 빨간 구
        Gizmos.color = new Color(1f, 0.2f, 0.2f, 0.9f);
        Gizmos.DrawSphere(transform.position, 0.4f);

        // 감지 범위 - 노란 원
        Gizmos.color = new Color(1f, 1f, 0f, 0.15f);
        Gizmos.DrawWireSphere(transform.position, detectionRange);

        // 공격 범위 - 빨간 원
        Gizmos.color = new Color(1f, 0f, 0f, 0.4f);
        Gizmos.DrawWireSphere(transform.position, attackRange);
    }

    void OnDrawGizmosSelected()
    {
        // 선택 시 인디케이터 시각화
        Gizmos.color = new Color(1f, 0.5f, 0f, 0.8f);

        if (indicatorType == IndicatorType.Circle)
        {
            Gizmos.DrawWireSphere(transform.position, hitRadius);
        }
        else if (indicatorType == IndicatorType.RushCircle || indicatorType == IndicatorType.RushCone)
        {
            Vector3 rushEnd = transform.position + transform.forward * rushDistance;
            Gizmos.DrawLine(transform.position, rushEnd);
            Gizmos.DrawWireSphere(rushEnd, hitRadius);
        }

#if UNITY_EDITOR
        // 이름 라벨
        UnityEditor.Handles.Label(
            transform.position + Vector3.up * 0.8f,
            $"{presetName} x{count}"
        );
#endif
    }
}

// ─────────────────────────────────────────
//  열거형 (C++ 쪽과 이름 맞춤)
// ─────────────────────────────────────────

public enum AttackType
{
    Melee,      // MeleeAttackBehavior
    RushFront,  // RushFrontAttackBehavior
    RushAoE,    // RushAoEAttackBehavior
    Ranged      // RangedAttackBehavior
}

public enum IndicatorType
{
    None,
    Circle,      // 근접: 적 주변 원
    RushCircle,  // 돌진 + AoE: 선 + 도착지 원
    RushCone     // 돌진 + 콘: 선 + 도착지 부채꼴
}
