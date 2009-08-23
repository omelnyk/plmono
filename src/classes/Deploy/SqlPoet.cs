using System;
using System.Text;
using System.Reflection;
using System.Collections.Generic;

namespace PLMono
{
	internal class SqlPoet
	{
		private const string CreateFunctionQuery =
			"CREATE FUNCTION {0}({1})\n" + 
			"    RETURNS {2}\n" + 
			"    AS '{3}'\n" + 
			"    LANGUAGE plmono;\n";

		private const string CreateTypePrototypeQuery =
			"CREATE TYPE {0}\n";

		private const string CreateTypeQuery =
			"CREATE TYPE {0} (\n" + 
			"    INPUT = {1}\n" + 
			"    OUTPUT = {2}\n" + 
			");\n";

		private const string CreateAggregateQuery =
			"CREATE AGGREGATE {0} (\n" + 
			"    BASETYPE = {1}\n" + 
			"    SFUNC = {2}\n" + 
			"    STYPE = {3}\n" + 
			");\n";

		private Dictionary<string,string> TypeMappings = new Dictionary<string,string>
		{
			{"System.Bool",   "boolean"         },
			{"System.Int16",  "smallint"        },
			{"System.Int32",  "integer"         },
			{"System.Int64",  "bigint"          },
			{"System.Single", "real"            },
			{"System.Double", "double precision"},
			{"System.String", "text"            }
		};

		private string DatabaseTypeName(Type type)
		{
			string name = type.ToString();

			try
			{
				if (IsSqlType(type))
				{
					return SqlTypeName(type);
				}

				return TypeMappings[name];
			}
			catch (KeyNotFoundException)
			{
				throw new NotImplementedException("Type " + name + " is not supported");
			}
		}

 		public bool IsSqlType(Type type)
		{
			object[] attributes = type.GetCustomAttributes(true);
			foreach (object attrib in attributes)
				if (attrib.GetType().ToString() == typeof(SqlType).FullName)
					return true;

			return false;
		}

		public string SqlTypeName(Type type)
		{
			return type.Name;
		}

		public bool IsSqlAggregate(Type type)
		{
			object[] attributes = type.GetCustomAttributes(true);
			foreach (object attrib in attributes)
				if (attrib.GetType().ToString() == typeof(SqlAggregate).FullName)
					return true;

			return false;
		}

		public bool IsSqlFunction(MethodInfo method)
		{
			object[] attributes = method.GetCustomAttributes(true);
			foreach (object attrib in attributes)
				if (attrib.GetType().ToString() == typeof(SqlFunction).FullName)
					return true;

			return false;
		}

		public string SqlFunctionName(MethodInfo method)
		{
			object[] attributes = method.GetCustomAttributes(true);
			foreach (object attrib in attributes)
				if (attrib is SqlFunction)
				{
					Console.WriteLine("Attribute type is: {0}", attrib.GetType().ToString());
					string attribName = ((SqlFunction) attrib).Name;
					Console.WriteLine("Success");
					if (attribName != string.Empty)
						return attribName;
					break;
				}

			return method.Name;
		}

		private string FullMethodName(MethodInfo method)
		{
			return method.DeclaringType.FullName + ":" + method.Name;
		}

		private string ArgumentsDeclaration(ParameterInfo[] args)
		{
			string[] typeNames = new string[args.Length];
			for (int i = 0; i < args.Length; i++)
			{
				typeNames[i] = DatabaseTypeName(args[i].ParameterType);
			}

			return string.Join(", ", typeNames);
		}

		public string FunctionDeclaration(MethodInfo method, string name)
		{
			return string.Format(CreateFunctionQuery, name,
				ArgumentsDeclaration(method.GetParameters()),
            	DatabaseTypeName(method.ReturnType), FullMethodName(method)
			);
		}

		public string FunctionDeclaration(MethodInfo method)
		{
			string name = SqlFunctionName(method);
			return FunctionDeclaration(method, name);
		}

		public string TypeDeclaration(string name, Type type)
		{
			MethodInfo inputFunc, outputFunc;
			string inputFuncName, outputFuncName;

			inputFuncName = name + "_input";
			inputFunc = type.GetMethod("Parse");

			outputFunc = type.GetMethod("ToString");
			outputFuncName = name + "_output";

			return string.Format(CreateTypePrototypeQuery, name) + 
				FunctionDeclaration(inputFunc, inputFuncName) +
				FunctionDeclaration(outputFunc, outputFuncName) + 
				string.Format(CreateTypeQuery, name, inputFuncName, outputFuncName);
		}

		public string AggregateDeclaration(string name, Type type)
		{
			return "";
		}
	}
}
