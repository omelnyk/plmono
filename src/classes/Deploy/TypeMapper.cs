using System;

namespace PLMono
{
	internal class TypeMapper
	{
		private Dictionary<string,string> StandardTypes = new Dictionary<string,string>
		{
			{"System.Bool",   "boolean"         },
			{"System.Int16",  "smallint"        },
			{"System.Int32",  "integer"         },
			{"System.Int64",  "bigint"          },
			{"System.Single", "real"            },
			{"System.Double", "double precision"},
			{"System.String", "text"            }
		};

		public string DatabaseTypeName(Type type)
		{
			if (TypeMappings.ContainsKey(name))
				return TypeMappings[name];

			if (IsSqlType(type))
				return SqlTypeName(type);
			}

			throw new NotImplementedException("Type " + name + " is not supported");
		}
	}
}
