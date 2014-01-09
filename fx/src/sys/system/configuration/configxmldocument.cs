//------------------------------------------------------------------------------
// <copyright file="ConfigXmlDocument.cs" company="Microsoft">
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
//------------------------------------------------------------------------------

namespace System.Configuration
{
    using System.Configuration.Internal;
    using System.IO;
    using System.Xml;
    using System.Security.Permissions;

    // ConfigXmlDocument - the default Xml Document doesn't track line numbers, and line
    // numbers are necessary to display source on config errors.
    // These classes wrap corresponding System.Xml types and also carry 
    // the necessary information for reporting filename / line numbers.
    // Note: these classes will go away if webdata ever decides to incorporate line numbers
    // into the default XML classes.  This class could also go away if webdata brings back
    // the UserData property to hang any info off of any node.
    [PermissionSet(SecurityAction.LinkDemand, Name="FullTrust")]
    public sealed class ConfigXmlDocument : XmlDocument, IConfigErrorInfo {
        XmlTextReader   _reader;
        int             _lineOffset;
        string          _filename;

        int IConfigErrorInfo.LineNumber {
            get { 
                if (_reader == null) {
                    return 0;
                }
                
                if (_lineOffset > 0) {
                    return _reader.LineNumber + _lineOffset - 1;
                }
                
                return _reader.LineNumber;
            }
        }

        public int LineNumber { get { return ((IConfigErrorInfo)this).LineNumber; } }

        public string Filename {
            get { return ConfigurationException.SafeFilename(_filename); }
        }

        string IConfigErrorInfo.Filename { 
            get { return _filename; } 
        }

        public override void Load(string filename) {
            _filename = filename;
            try {
                _reader = new XmlTextReader(filename);
                _reader.XmlResolver = null;
                base.Load(_reader);
            }
            finally {
                if (_reader != null) {
                    _reader.Close();
                    _reader = null;
                }
            }
        }


        public void LoadSingleElement(string filename, XmlTextReader sourceReader) {
            _filename = filename;
            _lineOffset = sourceReader.LineNumber;
            string outerXml = sourceReader.ReadOuterXml();
            
            try {
                _reader = new XmlTextReader(new StringReader(outerXml), sourceReader.NameTable);
                base.Load(_reader);
            }
            finally {
                if (_reader != null) {
                    _reader.Close();
                    _reader = null;
                }
            }
        }

        public override XmlAttribute CreateAttribute( string prefix, string localName, string namespaceUri ) {
            return new ConfigXmlAttribute( _filename, LineNumber, prefix, localName, namespaceUri, this );
        }
        public override XmlElement CreateElement( string prefix, string localName, string namespaceUri) {
            return new ConfigXmlElement( _filename, LineNumber, prefix, localName, namespaceUri, this );
        }
        public override XmlText CreateTextNode(String text) {
            return new ConfigXmlText( _filename, LineNumber, text, this );
        }
        public override XmlCDataSection CreateCDataSection(String data) {
            return new ConfigXmlCDataSection( _filename, LineNumber, data, this );
        }
        public override XmlComment CreateComment(String data) {
            return new ConfigXmlComment( _filename, LineNumber, data, this );
        }
        public override XmlSignificantWhitespace CreateSignificantWhitespace(String data) {
            return new ConfigXmlSignificantWhitespace( _filename, LineNumber, data, this );
        }
        public override XmlWhitespace CreateWhitespace(String data) {
            return new ConfigXmlWhitespace( _filename, LineNumber, data, this );
        }
    }
}
