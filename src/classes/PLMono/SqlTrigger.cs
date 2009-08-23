using System;

namespace PLMono
{
	[AttributeUsage (AttributeTargets.Method)]
	public class SqlTrigger : Attribute
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
