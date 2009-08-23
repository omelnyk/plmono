using System;
using System.Collections;
using System.Collections.Generic;

namespace PLMono
{
	public class TableRow : IEnumerable
	{
		private Dictionary<string,object> columns = 
			new Dictionary<string,object>();

		public int Count
		{
			get
			{
				return columns.Count;
			}
		}

		public object this[string name]
		{
			get
			{
				return columns[name];
			}
			set
			{
				if (columns.ContainsKey(name))
					columns[name] = value;
				else
					throw new InexistingColumnException(name);
			}
		}

		internal void Add(string name, object val)
		{
			columns.Add(name, val);
		}

		public IEnumerator GetEnumerator()
		{
			foreach (KeyValuePair<string,object> pair in columns)
				yield return pair; 
		}
	}
}
