using System;

namespace PLMono
{
	[AttributeUsage (AttributeTargets.Class)]
	public class SqlType : Attribute
	{
		private string name;

		public string Name
		{
			get
			{
				return name;
			}
			set
			{
				name = value;
			}
		}
	}
}
