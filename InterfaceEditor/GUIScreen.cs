﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing.Design;
using System.ComponentModel;

namespace InterfaceEditor
{
	class GUIScreen : GUIPanel
	{
		public bool IsModal { get; set; }
		public bool IsMultiinstance { get; set; }
		public bool IsCloseOnMiss { get; set; }
		public bool IsAutoCursor { get; set; }
		public string AutoCursorType { get; set; }

		public GUIScreen(GUIPanel parent)
			: base(parent)
		{
			IsModal = true;
			IsCanMove = true;
		}
	}
}
