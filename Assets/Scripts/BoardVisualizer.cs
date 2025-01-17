using UnityEngine;
using UnityEngine.UI;

public class BoardVisualizer : MonoBehaviour
{
    [SerializeField] private Image[] cellImages;


    public void SetCellColor(int x, int y, Color color)
    {
        if (x < 0 || x >= 8 || y < 0 || y >= 8) return;
        int index = y * 8 + x;
        if (index >= 0 && index < cellImages.Length)
        {
            cellImages[index].color = color;
        }
    }
    
    public void ResetBoard()
    {
        Color defaultColor = Color.gray;
        for (int i = 0; i < cellImages.Length; i++)
        {
            cellImages[i].color = defaultColor;
        }
    }
}