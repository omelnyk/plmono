using System;
using System.Reflection;

namespace PLMono
{
	internal class Deployer
	{
		private static SqlPoet poet = new SqlPoet();

		public static int Main(string[] args)
		{
			if (args.Length == 0)
			{
				Console.WriteLine("At least one assembly must be specified for analysis");
				return 0;
			}

			foreach (string filename in args)
			{
				Assembly library = Assembly.LoadFile(filename);
				Type[] types = library.GetTypes();

				foreach (Type type in types)
				{
					Console.Write("{0}: ", type.FullName);

					if (poet.IsSqlType(type))
						Console.WriteLine("Declaration: {0}", poet.TypeDeclaration(type.Name, type));

					if (poet.IsSqlAggregate(type))
						Console.WriteLine("Declaration: {0}", poet.AggregateDeclaration(type.Name, type));

					MethodInfo[] methods = type.GetMethods();
					foreach (MethodInfo method in methods)
					{
						if (poet.IsSqlFunction(method))
							Console.WriteLine("Declaration: {0}", poet.FunctionDeclaration(method));
					}
				}
			}


			return 0;
		}
	}
}
