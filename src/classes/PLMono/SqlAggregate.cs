using System;

namespace PLMono
{
	[AttributeUsage (AttributeTargets.Class)]
	public class SqlAggregate : Attribute
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
