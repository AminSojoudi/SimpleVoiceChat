using UnityEngine;

namespace Managers
{
    public class MenuManager : MonoBehaviour
    {
        public static MenuManager Instance;

        public GameObject ConnectionMenu;
        public GameObject ChatMenu;

        private void Awake()
        {
            Instance = this;
        }

    
    }
}
