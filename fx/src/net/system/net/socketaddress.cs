//------------------------------------------------------------------------------
// <copyright file="SocketAddress.cs" company="Microsoft">
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

namespace System.Net {

    using System;
    using System.Runtime.InteropServices;
    using System.Net.Sockets;
    using System.Text;
    using System.Globalization;

    // a little perf app measured these times when comparing the internal
    // buffer implemented as a managed byte[] or unmanaged memory IntPtr
    // that's why we use byte[]
    // byte[] total ms:19656
    // IntPtr total ms:25671

    /// <devdoc>
    ///    <para>
    ///       This class is used when subclassing EndPoint, and provides indication
    ///       on how to format the memeory buffers that winsock uses for network addresses.
    ///    </para>
    /// </devdoc>
    public class SocketAddress {

        internal const int IPv6AddressSize = 28;
        internal const int IPv4AddressSize = 16;

        internal int m_Size;
        internal byte[] m_Buffer;

        private const int WriteableOffset = 2;
        private const int MaxSize = 32; // IrDA requires 32 bytes
        private bool m_changed = true;
        private int m_hash;

        //
        // Address Family
        //
        /// <devdoc>
        ///    <para>[To be supplied.]</para>
        /// </devdoc>
        public AddressFamily Family {
            get {
                int family;
#if BIGENDIAN
                family = ((int)m_Buffer[0]<<8) | m_Buffer[1];
#else
                family = m_Buffer[0] | ((int)m_Buffer[1]<<8);
#endif
                return (AddressFamily)family;
            }
        }
        //
        // Size of this SocketAddress
        //
        /// <devdoc>
        ///    <para>[To be supplied.]</para>
        /// </devdoc>
        public int Size {
            get {
                return m_Size;
            }
        }

        //
        // access to unmanaged serialized data. this doesn't
        // allow access to the first 2 bytes of unmanaged memory
        // that are supposed to contain the address family which
        // is readonly.
        //
        //
        /// <devdoc>
        ///    <para>[To be supplied.]</para>
        /// </devdoc>
        public byte this[int offset] {
            get {
                //
                // access
                //
                if (offset<0 || offset>=Size) {
                    throw new IndexOutOfRangeException();
                }
                return m_Buffer[offset];
            }
            set {
                if (offset<0 || offset>=Size) {
                    throw new IndexOutOfRangeException();
                }
                if (m_Buffer[offset] != value) {
                    m_changed = true;
                }
                m_Buffer[offset] = value;
            }
        }

        /// <devdoc>
        ///    <para>[To be supplied.]</para>
        /// </devdoc>
        public SocketAddress(AddressFamily family) : this(family, MaxSize) {
        }

        /// <devdoc>
        ///    <para>[To be supplied.]</para>
        /// </devdoc>
        public SocketAddress(AddressFamily family, int size) {
            if (size<WriteableOffset) {
                //
                // it doesn't make sense to create a socket address with less tha
                // 2 bytes, that's where we store the address family.
                //
                throw new ArgumentOutOfRangeException("size");
            }
            m_Size = size;
            m_Buffer = new byte[(size/IntPtr.Size+2)*IntPtr.Size];//sizeof DWORD

#if BIGENDIAN
            m_Buffer[0] = unchecked((byte)((int)family>>8));
            m_Buffer[1] = unchecked((byte)((int)family   ));
#else
            m_Buffer[0] = unchecked((byte)((int)family   ));
            m_Buffer[1] = unchecked((byte)((int)family>>8));
#endif
        }
        //
        // For ReceiveFrom we need to pin address size, using reserved m_Buffer space
        //
        internal void CopyAddressSizeIntoBuffer()
        {
            m_Buffer[m_Buffer.Length-IntPtr.Size]   = unchecked((byte)(m_Size));
            m_Buffer[m_Buffer.Length-IntPtr.Size+1] = unchecked((byte)(m_Size >> 8));
            m_Buffer[m_Buffer.Length-IntPtr.Size+2] = unchecked((byte)(m_Size >> 16));
            m_Buffer[m_Buffer.Length-IntPtr.Size+3] = unchecked((byte)(m_Size >> 24));
        }
        //
        // Can be called after the above method did work
        //
        internal int GetAddressSizeOffset()
        {
            return m_Buffer.Length-IntPtr.Size;
        }
        //
        //
        // For ReceiveFrom we need to update the address size upon IO return
        //
        internal unsafe void SetSize(IntPtr ptr)
        {
            // Apparently it must be less or equal the original value since ReceiveFrom cannot reallocate the address buffer
            m_Size = *(int*)ptr;
        }
        public override bool Equals(object comparand) {
            SocketAddress castedComparand = comparand as SocketAddress;
            if (castedComparand == null || this.Size != castedComparand.Size) {
                return false;
            }
            for(int i=0; i<this.Size; i++) {
                if(this[i]!=castedComparand[i]) {
                    return false;
                }
            }
            return true;
        }

        public override int GetHashCode() {
            if (m_changed) {
                m_changed = false;
                m_hash = 0;

                int i;
                int size = Size & ~3;

                for (i = 0; i < size; i += 4) {
                    m_hash ^= (int)m_Buffer[i]
                            | ((int)m_Buffer[i+1] << 8)
                            | ((int)m_Buffer[i+2] << 16)
                            | ((int)m_Buffer[i+3] << 24);
                }
                if ((Size & 3) != 0) {

                    int remnant = 0;
                    int shift = 0;

                    for (; i < Size; ++i) {
                        remnant |= ((int)m_Buffer[i]) << shift;
                        shift += 8;
                    }
                    m_hash ^= remnant;
                }
            }
            return m_hash;
        }

        public override string ToString() {
            StringBuilder bytes = new StringBuilder();
            for(int i=WriteableOffset; i<this.Size; i++) {
                if (i>WriteableOffset) {
                    bytes.Append(",");
                }
                bytes.Append(this[i].ToString(NumberFormatInfo.InvariantInfo));
            }
            return Family.ToString() + ":" + Size.ToString(NumberFormatInfo.InvariantInfo) + ":{" + bytes.ToString() + "}";
        }

    } // class SocketAddress


} // namespace System.Net
