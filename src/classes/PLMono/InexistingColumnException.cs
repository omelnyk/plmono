using System;

namespace PLMono
{
	public class InexistingColumnException : Exception
	{
		private string name;

		public InexistingColumnException(string name)
		{
			Name = name;
		}

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
