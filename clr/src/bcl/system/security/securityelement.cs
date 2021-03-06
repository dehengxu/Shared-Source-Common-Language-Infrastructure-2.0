// ==++==
// 
//   
//    Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//   
//    The use and distribution terms for this software are contained in the file
//    named license.txt, which can be found in the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by the
//    terms of this license.
//   
//    You must not remove this notice, or any other, from this software.
//   
// 
// ==--==

namespace System.Security
{
    using System;
    using System.Collections;
    using System.Security.Util;
    using System.Text;
    using System.Globalization;
    using System.IO;
    using System.Security.Permissions;

    internal enum SecurityElementType
    {
        Regular = 0,
        Format = 1,
        Comment = 2
    }

    
    internal interface ISecurityElementFactory
    {
        SecurityElement CreateSecurityElement();

        Object Copy();

        String GetTag();

        String Attribute( String attributeName );
    }

    [Serializable]
[System.Runtime.InteropServices.ComVisible(true)]
    sealed public class SecurityElement : ISecurityElementFactory
    {
        internal String  m_strTag;
        internal String  m_strText;
        private ArrayList m_lChildren;
        internal ArrayList m_lAttributes;
        internal SecurityElementType m_type = SecurityElementType.Regular;
        
        private static readonly char[] s_tagIllegalCharacters = new char[] { ' ', '<', '>' };
        private static readonly char[] s_textIllegalCharacters = new char[] { '<', '>' };
        private static readonly char[] s_valueIllegalCharacters = new char[] { '<', '>', '\"' };
        private const String s_strIndent = "   ";
        
        private const int c_AttributesTypical = 4 * 2;  // 4 attributes, times 2 strings per attribute
        private const int c_ChildrenTypical = 1;  

        private static readonly String[] s_escapeStringPairs = new String[]
            {
                // these must be all once character escape sequences or a new escaping algorithm is needed
                "<", "&lt;",
                ">", "&gt;",
                "\"", "&quot;",
                "\'", "&apos;",
                "&", "&amp;"
            };

        private static readonly char[] s_escapeChars = new char[] { '<', '>', '\"', '\'', '&' };
        
        //-------------------------- Constructors ---------------------------
        
        internal SecurityElement()
        {
        }

////// ISecurityElementFactory implementation

        SecurityElement ISecurityElementFactory.CreateSecurityElement()
        {
            return this;
        }

        String ISecurityElementFactory.GetTag()
        {
            return ((SecurityElement)this).Tag;
        }

        Object ISecurityElementFactory.Copy()
        {
            return ((SecurityElement)this).Copy();
        }

        String ISecurityElementFactory.Attribute( String attributeName )
        {
            return ((SecurityElement)this).Attribute( attributeName );
        }

//////////////

        public static SecurityElement FromString( String xml )
        {
            if (xml == null)
                throw new ArgumentNullException( "xml" );

            return new Parser( xml ).GetTopElement();
        }
        
        public SecurityElement( String tag )
        {
            if (tag == null)
                throw new ArgumentNullException( "tag" );
        
            if (!IsValidTag( tag ))
                throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementTag" ), tag ) );
        
            m_strTag = tag;
            m_strText = null;
        }

        public SecurityElement( String tag, String text )
        {
            if (tag == null)
                throw new ArgumentNullException( "tag" );
        
            if (!IsValidTag( tag ))
                throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementTag" ), tag ) );

            if (text != null && !IsValidText( text ))
                throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementText" ), text ) );
        
            m_strTag = tag;
            m_strText = text;
        }
    
        //-------------------------- Properties -----------------------------

        public String Tag
        {
            get
            {
                return m_strTag;
            }
            
            set
            {
                if (value == null)
                    throw new ArgumentNullException( "Tag" );
        
                if (!IsValidTag( value ))
                    throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementTag" ), value ) );
            
                m_strTag = value;
            }
        }

        public System.Collections.Hashtable Attributes
        {
            get
            {
                if (m_lAttributes == null || m_lAttributes.Count == 0)
                {
                    return null;
                }
                else
                {
                    System.Collections.Hashtable hashtable = new System.Collections.Hashtable( m_lAttributes.Count );
                                        
                    int iMax = m_lAttributes.Count;
                    BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );
                          
                    for (int i = 0; i < iMax; i += 2)
                    {
                        hashtable.Add( m_lAttributes[i], m_lAttributes[i+1]);
                    }
                                        
                    return hashtable;
                }
            }
            
            set
            {
                if (value == null || value.Count == 0)
                {
                    m_lAttributes = null;
                }
                else
                {
                    ArrayList list = new ArrayList( value.Count );
                    
                    System.Collections.IDictionaryEnumerator enumerator = (System.Collections.IDictionaryEnumerator)value.GetEnumerator();
                    
                    while (enumerator.MoveNext())
                    {
                        String attrName = (String)enumerator.Key;
                        String attrValue = (String)enumerator.Value;
                    
                        if (!IsValidAttributeName( attrName ))
                            throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementName" ), (String)enumerator.Current ) );
                        
                        if (!IsValidAttributeValue( attrValue ))
                            throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementValue" ), (String)enumerator.Value ) );
                    
                        list.Add(attrName);
                        list.Add(attrValue);
                    }
                    
                    m_lAttributes = list;
                }
            }
        }

        public String Text
        {
            get
            {
                return Unescape( m_strText );
            }
            
            set
            {
                if (value == null)
                {
                    m_strText = null;
                }
                else
                {
                    if (!IsValidText( value ))
                        throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementTag" ), value ) );
                        
                    m_strText = value;
                }
            }
        }

        public ArrayList Children
        {
            get
            {
                ConvertSecurityElementFactories();
                return m_lChildren;
            }
            
            set
            {
                if (value != null)
                {
                    IEnumerator enumerator = value.GetEnumerator();
                    
                    while (enumerator.MoveNext())
                    {
                        if (enumerator.Current == null)
                            throw new ArgumentException( Environment.GetResourceString( "ArgumentNull_Child" ) );
                    }
                }
            
                m_lChildren = value;
            }
        }

        internal void ConvertSecurityElementFactories()
        {
            if (m_lChildren == null)
                return;

            for (int i = 0; i < m_lChildren.Count; ++i)
            {
                ISecurityElementFactory iseFactory = m_lChildren[i] as ISecurityElementFactory;
                if (iseFactory != null && !(m_lChildren[i] is SecurityElement))
                    m_lChildren[i] = iseFactory.CreateSecurityElement();
            }
        }

        internal ArrayList InternalChildren
        {
            get
            {
                // Beware!  This array list can contain SecurityElements and other ISecurityElementFactories.
                // If you want to get a consistent SecurityElement view, call get_Children.
                return m_lChildren;
            }
        }
     
        //-------------------------- Public Methods -----------------------------
        
        internal void AddAttributeSafe( String name, String value )
        {
            if (m_lAttributes == null)
            {
                m_lAttributes = new ArrayList( c_AttributesTypical  );
            }
            else
            {
                int iMax = m_lAttributes.Count;
                BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );
                          
                for (int i = 0; i < iMax; i += 2)
                {
                    String strAttrName = (String)m_lAttributes[i];
                    
                    if (String.Equals(strAttrName, name))
                        throw new ArgumentException( Environment.GetResourceString( "Argument_AttributeNamesMustBeUnique" ) );
                }
            }
        
            m_lAttributes.Add(name);
            m_lAttributes.Add(value);
        }        
        
        public void AddAttribute( String name, String value )
        {   
            if (name == null)
                throw new ArgumentNullException( "name" );
                
            if (value == null)
                throw new ArgumentNullException( "value" );
        
            if (!IsValidAttributeName( name ))
                throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementName" ), name ) );
                
            if (!IsValidAttributeValue( value ))
                throw new ArgumentException( String.Format( CultureInfo.CurrentCulture, Environment.GetResourceString( "Argument_InvalidElementValue" ), value ) );
        
            AddAttributeSafe( name, value );
        }

        public void AddChild( SecurityElement child )
        {   
            if (child == null)
                throw new ArgumentNullException( "child" );
        
            if (m_lChildren == null)
                m_lChildren = new ArrayList( c_ChildrenTypical  );
                
            m_lChildren.Add( child );
        }

        internal void AddChild( ISecurityElementFactory child )
        {
            if (child == null)
                throw new ArgumentNullException( "child" );
        
            if (m_lChildren == null)
                m_lChildren = new ArrayList( c_ChildrenTypical  );
                
            m_lChildren.Add( child );
        }            

        internal void AddChildNoDuplicates( ISecurityElementFactory child )
        {   
            if (child == null)
                throw new ArgumentNullException( "child" );
        
            if (m_lChildren == null)
            {
                m_lChildren = new ArrayList( c_ChildrenTypical  );
                m_lChildren.Add( child );
            }
            else
            {
                for (int i = 0; i < m_lChildren.Count; ++i)
                {
                    if (m_lChildren[i] == child)
                        return;
                }
                m_lChildren.Add( child );
            }
        }

        public bool Equal( SecurityElement other )
        {
            if (other == null)
                return false;
        
            // Check if the tags are the same
            if (!String.Equals(m_strTag, other.m_strTag))
                return false;

            // Check if the text is the same
            if (!String.Equals(m_strText, other.m_strText))
                return false;

            // Check if the attributes are the same and appear in the same
            // order.
            
            // Maybe we can get away by only checking the number of attributes
            if (m_lAttributes == null || other.m_lAttributes == null)
            {
                if (m_lAttributes != other.m_lAttributes)
                    return false;
            }
            else 
            {				
                int iMax = m_lAttributes.Count;
                BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );

                if (iMax != other.m_lAttributes.Count)
                    return false;
                          
                for (int i = 0; i < iMax; i++)
                {
				    String lhs = (String)m_lAttributes[i];
				    String rhs = (String)other.m_lAttributes[i];
				    
				    if (!String.Equals(lhs, rhs))
				        return false;
                }
			}

            // Finally we must check the child and make sure they are
            // equal and in the same order
            
            // Maybe we can get away by only checking the number of children
            if (m_lChildren == null || other.m_lChildren == null) 
			{
				if (m_lChildren != other.m_lChildren)
	                return false;
			} 
			else 
			{
				if (m_lChildren.Count != other.m_lChildren.Count)
					return false;

                this.ConvertSecurityElementFactories();
                other.ConvertSecurityElementFactories();

				// Okay, we'll need to go through each one of them
				IEnumerator lhs = m_lChildren.GetEnumerator();
				IEnumerator rhs = other.m_lChildren.GetEnumerator();

				SecurityElement e1, e2;
				while (lhs.MoveNext())
				{
					rhs.MoveNext();
					e1 = (SecurityElement)lhs.Current;
					e2 = (SecurityElement)rhs.Current;       
					if (e1 == null || !e1.Equal(e2))                
						return false;
				}
			}

            return true;
        }

        [System.Runtime.InteropServices.ComVisible(false)]
        public SecurityElement Copy()
        {
            SecurityElement element = new SecurityElement( this.m_strTag, this.m_strText );

            element.m_lChildren = this.m_lChildren == null ? null : new ArrayList( this.m_lChildren );
            element.m_lAttributes = this.m_lAttributes == null ? null : new ArrayList( this.m_lAttributes );

            return element;
        }

        public static bool IsValidTag( String tag )
        {
            if (tag == null)
                return false;
                
            return tag.IndexOfAny( s_tagIllegalCharacters ) == -1;
        }

        public static bool IsValidText( String text )
        {
            if (text == null)
                return false;
                
            return text.IndexOfAny( s_textIllegalCharacters ) == -1;
        }

        public static bool IsValidAttributeName( String name )
        {
            return IsValidTag( name );
        }
        
        public static bool IsValidAttributeValue( String value )
        {
            if (value == null)
                return false;
                
            return value.IndexOfAny( s_valueIllegalCharacters ) == -1;
        }

        private static String GetEscapeSequence( char c )
        {
            int iMax = s_escapeStringPairs.Length;
            BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );
                          
            for (int i = 0; i < iMax; i += 2)
            {
                String strEscSeq = s_escapeStringPairs[i];
                String strEscValue = s_escapeStringPairs[i+1];

                if (strEscSeq[0] == c)
                    return strEscValue;
            }

            BCLDebug.Assert( false, "Unable to find escape sequence for this character" );
            return c.ToString();
        }

        public static String Escape( String str )
        {
            if (str == null)
                return null;

            StringBuilder sb = null;

            String temp = str;
            int index;

            do
            {
                index = temp.IndexOfAny( s_escapeChars );

                if (index == -1)
                {
                    if (sb == null)
                        return str;
                    else
                    {
                        sb.Append( temp );
                        return sb.ToString();
                    }
                }
                else
                {
                    if (sb == null)
                        sb = new StringBuilder();                    

                    sb.Append( temp.Substring( 0, index ) );
                    sb.Append( GetEscapeSequence( temp[index] ) );

                    temp = temp.Substring( index + 1 );
                }
            }
            while (true);
            
            // no normal exit is possible
        }

        private static String GetUnescapeSequence( String str, int index, out int newIndex )
        {
            int maxCompareLength = str.Length - index;

            int iMax = s_escapeStringPairs.Length;
            BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );
                          
            for (int i = 0; i < iMax; i += 2)
            {
                String strEscSeq = s_escapeStringPairs[i];
                String strEscValue = s_escapeStringPairs[i+1];
                
                int length = strEscValue.Length;

                if (length <= maxCompareLength && String.Compare( strEscValue, 0, str, index, length, StringComparison.Ordinal) == 0)
                {
                    newIndex = index + strEscValue.Length;
                    return strEscSeq;
                }
            }

            newIndex = index + 1;
            return str[index].ToString();
        }
            

        private static String Unescape( String str )
        {
            if (str == null)
                return null;

            StringBuilder sb = null;

            String temp = str;
            int index;
            int newIndex;

            do
            {
                index = temp.IndexOf( '&' );

                if (index == -1)
                {
                    if (sb == null)
                        return str;
                    else
                    {
                        sb.Append( temp );
                        return sb.ToString();
                    }
                }
                else
                {
                    if (sb == null)
                        sb = new StringBuilder();

                    sb.Append( temp.Substring( 0, index ) );
                    sb.Append( GetUnescapeSequence( temp, index, out newIndex ) );

                    temp = temp.Substring( newIndex );
                }
            }
            while (true);

            // C# reports a warning if I leave this in, but I still kinda want to just in case.
            // BCLDebug.Assert( false, "If you got here, the execution engine or compiler is really confused" );
            // return str;
        }

        private delegate void ToStringHelperFunc( Object obj, String str );

        private static void ToStringHelperStringBuilder( Object obj, String str )
        {
            ((StringBuilder)obj).Append( str );
        }
        
        private static void ToStringHelperStreamWriter( Object obj, String str )
        {
            ((StreamWriter)obj).Write( str );
        }

        public override String ToString ()
        {
            StringBuilder sb = new StringBuilder();

            ToString( "", sb, new ToStringHelperFunc( ToStringHelperStringBuilder ) );

            return sb.ToString();
        }

        internal void ToWriter( StreamWriter writer )
        {
            ToString( "", writer, new ToStringHelperFunc( ToStringHelperStreamWriter ) );
        }
        
        private void ToString( String indent, Object obj, ToStringHelperFunc func )
        {
            // First add the indent
            
            // func( obj, indent );                       
            
            // Add in the opening bracket and the tag.
            
            func( obj, "<" );

            switch (m_type)
            {
                case SecurityElementType.Format:
                    func( obj, "?" );
                    break;

                case SecurityElementType.Comment:
                    func( obj, "!" );
                    break;

                default:
                    break;
            }

            func( obj, m_strTag );
            
            // If there are any attributes, plop those in.
            
            if (m_lAttributes != null && m_lAttributes.Count > 0)
            {
                func( obj, " " );

                int iMax = m_lAttributes.Count;
                BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );
                          
                for (int i = 0; i < iMax; i += 2)
                {
                    String strAttrName = (String)m_lAttributes[i];
                    String strAttrValue = (String)m_lAttributes[i+1];
                    
                    func( obj, strAttrName );
                    func( obj, "=\"" );
                    func( obj, strAttrValue );
                    func( obj, "\"" );

                    if (i != m_lAttributes.Count - 2)
                    {
                        if (m_type == SecurityElementType.Regular)
                        {
                            func( obj, Environment.NewLine );
                        }
                        else
                        {
                            func( obj, " " );
                        }
                    }
                }
            }
            
            if (m_strText == null && (m_lChildren == null || m_lChildren.Count == 0))
            {
                // If we are a single tag with no children, just add the end of tag text.
    
                switch (m_type)
                {
                    case SecurityElementType.Comment:
                        func( obj, ">" );
                        break;

                    case SecurityElementType.Format:
                        func( obj, " ?>" );
                        break;

                    default:
                        func( obj, "/>" );
                        break;
                }
                func( obj, Environment.NewLine );
            }
            else
            {
                // Close the current tag.
            
                func( obj, ">" );
                
                // Output the text
                
                func( obj, m_strText );
                
                // Output any children.
                
                if (m_lChildren != null)
                {
                    this.ConvertSecurityElementFactories();

                    func( obj, Environment.NewLine );
                
                    // String childIndent = indent + s_strIndent;
                
                    for (int i = 0; i < m_lChildren.Count; ++i)                    
                    {
                        ((SecurityElement)m_lChildren[i]).ToString( "", obj, func );
                    }

                    // In the case where we have children, the close tag will not be on the same line as the
                    // opening tag, so we need to indent.

                    // func( obj, indent );
                }
                
                // Output the closing tag
                
                func( obj, "</" );
                func( obj, m_strTag );
                func( obj, ">" );
                func( obj, Environment.NewLine );
            }
        }
                
                

        public String Attribute( String name )
        {
            if (name == null)
                throw new ArgumentNullException( "name" );
                
            // Note: we don't check for validity here because an
            // if an invalid name is passed we simply won't find it.
        
            if (m_lAttributes == null)
                return null;
                            
            // Go through all the attribute and see if we know about
            // the one we are asked for

            int iMax = m_lAttributes.Count;
            BCLDebug.Assert( iMax % 2 == 0, "Odd number of strings means the attr/value pairs were not added correctly" );
                          
            for (int i = 0; i < iMax; i += 2)
            {
                String strAttrName = (String)m_lAttributes[i];
                
                if (String.Equals(strAttrName, name))
                {
                    String strAttrValue = (String)m_lAttributes[i+1];
                    
                    return Unescape(strAttrValue);
                }
            }

            // In the case where we didn't find it, we are expected to
            // return null
            return null;
        }

        public SecurityElement SearchForChildByTag( String tag )
        {
            // Go through all the children and see if we can
            // find the one are are asked for (matching tags)

            if (tag == null)
                throw new ArgumentNullException( "tag" );
                
            // Note: we don't check for a valid tag here because
            // an invalid tag simply won't be found.    
                
            if (m_lChildren == null)
                return null;

            IEnumerator enumerator = m_lChildren.GetEnumerator();

            while (enumerator.MoveNext())
            {
                SecurityElement current = (SecurityElement)enumerator.Current;
            
                if (current != null && String.Equals(current.Tag, tag))
                    return current;
            }

            return null;
        }

        internal IPermission ToPermission(bool ignoreTypeLoadFailures)
        {
            IPermission ip = XMLUtil.CreatePermission( this, PermissionState.None, ignoreTypeLoadFailures );
            if (ip == null)
                return null;
            ip.FromXml(this);

            // Get the permission token here to ensure that the token
            // type is updated appropriately now that we've loaded the type.
            PermissionToken token = PermissionToken.GetToken( ip );
            BCLDebug.Assert((token.m_type & PermissionTokenType.DontKnow) == 0, "Token type not properly assigned");

            return ip;            
        }

        internal Object ToSecurityObject()
        {
            switch (m_strTag)
            {
                case "PermissionSet":
                    PermissionSet pset = new PermissionSet(PermissionState.None);
                    pset.FromXml(this);
                    return pset;

                default:
                    return ToPermission(false);
            }
        }

        internal String SearchForTextOfLocalName(String strLocalName) 
        {
            // Search on each child in order and each
            // child's child, depth-first
            
            if (strLocalName == null)
                throw new ArgumentNullException( "strLocalName" );
                
            // Note: we don't check for a valid tag here because
            // an invalid tag simply won't be found.    
            
            // First we check this.
			
			if (m_strTag == null) return null;            
            if (m_strTag.Equals( strLocalName ) || m_strTag.EndsWith( ":" + strLocalName, StringComparison.Ordinal ))
                return Unescape( m_strText );
                
            if (m_lChildren == null)
                return null;

            IEnumerator enumerator = m_lChildren.GetEnumerator();

            while (enumerator.MoveNext())
            {
                String current = ((SecurityElement)enumerator.Current).SearchForTextOfLocalName( strLocalName );
            
                if (current != null)
                    return current;
            }

            return null;            
        }

        public String SearchForTextOfTag( String tag )
        {
            // Search on each child in order and each
            // child's child, depth-first
            
            if (tag == null)
                throw new ArgumentNullException( "tag" );
                
            // Note: we don't check for a valid tag here because
            // an invalid tag simply won't be found.    
            
            // First we check this.
            
            if (String.Equals(m_strTag, tag))
                return Unescape( m_strText );
                
            if (m_lChildren == null)
                return null;

            IEnumerator enumerator = m_lChildren.GetEnumerator();

            this.ConvertSecurityElementFactories();

            while (enumerator.MoveNext())
            {
                String current = ((SecurityElement)enumerator.Current).SearchForTextOfTag( tag );
            
                if (current != null)
                    return current;
            }

            return null;
        } 
    }                        
}
