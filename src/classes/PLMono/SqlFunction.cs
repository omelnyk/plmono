using System;

namespace PLMono
{
	[AttributeUsage (AttributeTargets.Method)]
	public class SqlFunction : Attribute
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
