import type { Metadata } from 'next';
import { Geist, Geist_Mono } from 'next/font/google';
import './globals.css';

const geistSans = Geist({
  variable: '--font-geist-sans',
  subsets: ['latin'],
});

const geistMono = Geist_Mono({
  variable: '--font-geist-mono',
  subsets: ['latin'],
});

export const metadata: Metadata = {
  metadataBase: new URL('https://domes-product-status.pcesar22.chatgpt.site'),
  title: 'DOMES Product Status',
  description: 'Evidence-led DOMES product realization status from P0 foundation through open product release.',
  openGraph: {
    title: 'DOMES Product Status',
    description: 'From prototype evidence to open product release',
    type: 'website',
    images: [{ url: '/og.png', width: 1200, height: 630, alt: 'DOMES Product Status — from prototype evidence to open product release' }],
  },
  twitter: {
    card: 'summary_large_image',
    title: 'DOMES Product Status',
    description: 'From prototype evidence to open product release',
    images: ['/og.png'],
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body
        className={`${geistSans.variable} ${geistMono.variable} antialiased`}
      >
        {children}
      </body>
    </html>
  );
}
