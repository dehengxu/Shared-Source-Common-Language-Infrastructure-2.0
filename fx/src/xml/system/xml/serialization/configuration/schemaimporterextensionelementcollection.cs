//------------------------------------------------------------------------------
// <copyright file="SchemaImporterExtensionElementCollection.cs" company="Microsoft Corporation">
//     
//      Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//     
//      The use and distribution terms for this software are contained in the file
//      named license.txt, which can be found in the root of this distribution.
//      By using this software in any fashion, you are agreeing to be bound by the
//      terms of this license.
//     
//      You must not remove this notice, or any other, from this software.
//     
// </copyright>
// <owner current="true" primary="true">ElenaK</owner>                                                                
//------------------------------------------------------------------------------

namespace System.Xml.Serialization.Configuration
{

    using System;
    using System.Configuration;
    using System.Security.Permissions;

    [ConfigurationCollection(typeof(SchemaImporterExtensionElement))]
    public sealed class SchemaImporterExtensionElementCollection : ConfigurationElementCollection
    {
        public SchemaImporterExtensionElementCollection() 
        {
        }
        
        public SchemaImporterExtensionElement this[int index]
        {
            get
            {
                return (SchemaImporterExtensionElement)BaseGet(index);
            }
            set
            {
                if (BaseGet(index) != null)
                {
                    BaseRemoveAt(index);
                }
                BaseAdd(index,value);
            }
        }
         
        public new SchemaImporterExtensionElement this[string name]
        {
            get
            {
                return (SchemaImporterExtensionElement)BaseGet(name);
            }
            set
            {
                if (BaseGet(name) != null)
                {
                    BaseRemove(name);
                }
                BaseAdd(value);
            }
        }
         
        public void Add(SchemaImporterExtensionElement element)
        {
            BaseAdd(element);
        }

        public void Clear()
        {
            BaseClear();
        }

        protected override ConfigurationElement CreateNewElement() 
        {
            return new SchemaImporterExtensionElement();
        }

        protected override Object GetElementKey(ConfigurationElement element) 
        {
            return ((SchemaImporterExtensionElement)element).Key;
        }

        public int IndexOf(SchemaImporterExtensionElement element)
        {
            return BaseIndexOf(element);
        }
         
        public void Remove(SchemaImporterExtensionElement element) 
        {
            BaseRemove(element.Key);
        }

        public void Remove(string name) 
        {
            BaseRemove(name);
        }

        public void RemoveAt(int index)
        {
            BaseRemoveAt(index);
        }

    } 

}


