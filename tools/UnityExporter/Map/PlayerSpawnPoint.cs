using UnityEngine;

/// <summary>
/// 플레이어 시작 위치. 씬에 하나만 배치.
/// Transform의 position, rotation(Y축)을 그대로 익스포트에 사용.
/// </summary>
public class PlayerSpawnPoint : MonoBehaviour
{
    [Header("스폰 설정")]
    [Tooltip("씬당 하나만 배치할 것. 여러 개면 첫 번째만 사용됨.")]
    public bool isDefault = true;

    void OnDrawGizmos()
    {
        // 파란 구로 위치 표시
        Gizmos.color = new Color(0.2f, 0.5f, 1.0f, 0.9f);
        Gizmos.DrawSphere(transform.position, 0.5f);

        // 전방 방향 화살표
        Gizmos.color = new Color(0.2f, 1.0f, 0.4f, 0.9f);
        Gizmos.DrawRay(transform.position, transform.forward * 2.0f);
    }

    void OnDrawGizmosSelected()
    {
        // 선택 시 더 크게 표시
        Gizmos.color = new Color(0.2f, 0.5f, 1.0f, 0.4f);
        Gizmos.DrawWireSphere(transform.position, 0.8f);

#if UNITY_EDITOR
        UnityEditor.Handles.Label(
            transform.position + Vector3.up * 1.0f,
            "Player Start"
        );
#endif
    }
}
